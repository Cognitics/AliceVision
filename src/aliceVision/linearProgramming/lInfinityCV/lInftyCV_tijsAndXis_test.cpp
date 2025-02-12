// This file is part of the AliceVision project.
// Copyright (c) 2016 AliceVision contributors.
// Copyright (c) 2012 openMVG contributors.
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file,
// You can obtain one at https://mozilla.org/MPL/2.0/.

#include "aliceVision/multiview/NViewDataSet.hpp"
#include "aliceVision/numeric/numeric.hpp"
#include <aliceVision/config.hpp>
#include "aliceVision/numeric/projection.hpp"
#include "aliceVision/linearProgramming/ISolver.hpp"
#include "aliceVision/linearProgramming/OSIXSolver.hpp"
#if ALICEVISION_IS_DEFINED(ALICEVISION_HAVE_MOSEK)
    #include "aliceVision/linearProgramming/MOSEKSolver.hpp"
#endif

#include "aliceVision/linearProgramming/bisectionLP.hpp"
#include "aliceVision/linearProgramming/lInfinityCV/tijsAndXis_From_xi_Ri.hpp"

#include <iostream>
#include <vector>

#define BOOST_TEST_MODULE TranslationStructureLInfinity

#include <boost/test/unit_test.hpp>
#include <boost/test/tools/floating_point_comparison.hpp>

using namespace aliceVision;

using namespace linearProgramming;
using namespace lInfinityCV;

BOOST_AUTO_TEST_CASE(Translation_Structure_L_Infinity_OSICLP_SOLVER)
{
    const size_t nViews = 3;
    const size_t nbPoints = 6;
    const NViewDataSet d =
      NRealisticCamerasRing(nViews, nbPoints, NViewDatasetConfigurator(1, 1, 0, 0, 5, 0));  // Suppose a camera with Unit matrix as K

    d.exportToPLY("test_Before_Infinity.ply");
    //-- Test triangulation of all the point
    NViewDataSet d2 = d;

    //-- Set to 0 the future computed data to be sure of computation results :
    d2._X.fill(0);  // Set _Xi of dataset 2 to 0 to be sure of new data computation
    fill(d2._t.begin(), d2._t.end(), Vec3(0.0, 0.0, 0.0));

    // Create the mega matrix
    Mat megaMat(4, d._n * d._x[0].cols());
    {
        size_t cpt = 0;
        for (size_t i = 0; i < d._n; ++i)
        {
            const size_t camIndex = i;
            for (size_t j = 0; j < d._x[0].cols(); ++j)
            {
                megaMat(0, cpt) = d._x[camIndex].col(j)(0);
                megaMat(1, cpt) = d._x[camIndex].col(j)(1);
                megaMat(2, cpt) = j;
                megaMat(3, cpt) = camIndex;
                cpt++;
            }
        }
    }

    // Solve the problem and check that fitted value are good enough
    {
        std::vector<double> vec_solution((nViews + nbPoints) * 3);

        OSI_CISolverWrapper wrapperOSICLPSolver(vec_solution.size());
        Translation_Structure_L1_ConstraintBuilder cstBuilder(d._R, megaMat);
        BOOST_CHECK(
          (BisectionLP<Translation_Structure_L1_ConstraintBuilder, LPConstraintsSparse>(wrapperOSICLPSolver, cstBuilder, &vec_solution, 1.0, 0.0)));

        // Move computed value to dataset for residual estimation.
        {
            //-- Fill the ti
            for (size_t i = 0; i < nViews; ++i)
            {
                size_t index = i * 3;
                d2._t[i] = Vec3(vec_solution[index], vec_solution[index + 1], vec_solution[index + 2]);
                // Change Ci to -Ri*Ci
                d2._C[i] = -d2._R[i] * d2._t[i];
            }

            //-- Now the Xi :
            for (size_t i = 0; i < nbPoints; ++i)
            {
                size_t index = 3 * nViews;
                d2._X.col(i) = Vec3(vec_solution[index + i * 3], vec_solution[index + i * 3 + 1], vec_solution[index + i * 3 + 2]);
            }
        }

        // Compute residuals L2 from estimated parameter values :
        Vec2 xk, xsum(0.0, 0.0);
        for (size_t i = 0; i < d2._n; ++i)
        {
            for (size_t k = 0; k < d._x[0].cols(); ++k)
            {
                xk = project(d2.P(i), Vec3(d2._X.col(k)));
                xsum += Vec2((xk - d2._x[i].col(k)).array().pow(2));
            }
        }
        double dResidual2D = (xsum.array().sqrt().sum());

        // Check that 2D re-projection and 3D point are near to GT.
        BOOST_CHECK_SMALL(dResidual2D, 1e-4);
    }

    d2.exportToPLY("test_After_Infinity.ply");
}

BOOST_AUTO_TEST_CASE(Translation_Structure_L_Infinity_OSICLP_SOLVER_K)
{
    const size_t nViews = 3;
    const size_t nbPoints = 6;
    const NViewDataSet d =
      NRealisticCamerasRing(nViews, nbPoints, NViewDatasetConfigurator(1000, 1000, 500, 500, 5, 0));  // Suppose a camera with Unit matrix as K

    d.exportToPLY("test_Before_Infinity.ply");
    //-- Test triangulation of all the point
    NViewDataSet d2 = d;

    //-- Set to 0 the future computed data to be sure of computation results :
    d2._X.fill(0);  // Set _Xi of dataset 2 to 0 to be sure of new data computation
    fill(d2._t.begin(), d2._t.end(), Vec3(0.0, 0.0, 0.0));

    // Create the mega matrix
    Mat megaMat(4, d._n * d._x[0].cols());
    {
        size_t cpt = 0;
        for (size_t i = 0; i < d._n; ++i)
        {
            const size_t camIndex = i;
            for (size_t j = 0; j < (size_t)d._x[0].cols(); ++j)
            {
                megaMat(0, cpt) = d._x[camIndex].col(j)(0);
                megaMat(1, cpt) = d._x[camIndex].col(j)(1);
                megaMat(2, cpt) = j;
                megaMat(3, cpt) = camIndex;
                cpt++;
            }
        }
    }

    // Solve the problem and check that fitted value are good enough
    {
        std::vector<double> vec_solution((nViews + nbPoints) * 3);

        std::vector<Mat3> vec_KR(d._R);
        for (int i = 0; i < nViews; ++i)
            vec_KR[i] = d._K[0] * d._R[i];

        OSI_CISolverWrapper wrapperOSICLPSolver(vec_solution.size());
        Translation_Structure_L1_ConstraintBuilder cstBuilder(vec_KR, megaMat);
        BOOST_CHECK(
          (BisectionLP<Translation_Structure_L1_ConstraintBuilder, LPConstraintsSparse>(wrapperOSICLPSolver, cstBuilder, &vec_solution, 1.0, 0.0)));

        // Move computed value to dataset for residual estimation.
        {
            //-- Fill the ti
            for (size_t i = 0; i < nViews; ++i)
            {
                size_t index = i * 3;
                d2._t[i] = d._K[0].inverse() * Vec3(vec_solution[index], vec_solution[index + 1], vec_solution[index + 2]);
                // Change Ci to -Ri*Ci
                d2._C[i] = -d2._R[i] * d2._t[i];
            }

            //-- Now the Xi :
            for (size_t i = 0; i < nbPoints; ++i)
            {
                size_t index = 3 * nViews;
                d2._X.col(i) = Vec3(vec_solution[index + i * 3], vec_solution[index + i * 3 + 1], vec_solution[index + i * 3 + 2]);
            }
        }

        // Compute residuals L2 from estimated parameter values :
        Vec2 xk, xsum(0.0, 0.0);
        for (size_t i = 0; i < d2._n; ++i)
        {
            for (size_t k = 0; k < (size_t)d._x[0].cols(); ++k)
            {
                xk = project(d2.P(i), Vec3(d2._X.col(k)));
                xsum += Vec2((xk - d2._x[i].col(k)).array().pow(2));
            }
        }
        double dResidual2D = (xsum.array().sqrt().sum());

        // Check that 2D re-projection and 3D point are near to GT.
        BOOST_CHECK_SMALL(dResidual2D, 1e-4);
    }

    d2.exportToPLY("test_After_Infinity.ply");
}

#if ALICEVISION_IS_DEFINED(ALICEVISION_HAVE_MOSEK)
BOOST_AUTO_TEST_CASE(Translation_Structure_L_Infinity_MOSEK)
{
    const size_t nViews = 3;
    const size_t nbPoints = 6;
    const NViewDataSet d =
      NRealisticCamerasRing(nViews, nbPoints, NViewDatasetConfigurator(1, 1, 0, 0, 5, 0));  // Suppose a camera with Unit matrix as K

    d.exportToPLY("test_Before_Infinity.ply");
    //-- Test triangulation of all the point
    NViewDataSet d2 = d;

    //-- Set to 0 the future computed data to be sure of computation results :
    d2._X.fill(0);  // Set _Xi of dataset 2 to 0 to be sure of new data computation
    fill(d2._t.begin(), d2._t.end(), Vec3(0.0, 0.0, 0.0));

    // Create the mega matrix
    Mat megaMat(4, d._n * d._x[0].cols());
    {
        size_t cpt = 0;
        for (size_t i = 0; i < d._n; ++i)
        {
            const size_t camIndex = i;
            for (size_t j = 0; j < d._x[0].cols(); ++j)
            {
                megaMat(0, cpt) = d._x[camIndex].col(j)(0);
                megaMat(1, cpt) = d._x[camIndex].col(j)(1);
                megaMat(2, cpt) = j;
                megaMat(3, cpt) = camIndex;
                cpt++;
            }
        }
    }

    // Solve the problem and check that fitted value are good enough
    {
        std::vector<double> vec_solution((nViews + nbPoints) * 3);

        MOSEKSolver wrapperMosek(vec_solution.size());
        Translation_Structure_L1_ConstraintBuilder cstBuilder(d._R, megaMat);
        BOOST_CHECK(
          (BisectionLP<Translation_Structure_L1_ConstraintBuilder, LPConstraintsSparse>(wrapperMosek, cstBuilder, &vec_solution, 1.0, 0.0)));

        // Move computed value to dataset for residual estimation.
        {
            //-- Fill the ti
            for (size_t i = 0; i < nViews; ++i)
            {
                size_t index = i * 3;
                d2._t[i] = Vec3(vec_solution[index], vec_solution[index + 1], vec_solution[index + 2]);
                // Change Ci to -Ri*Ci
                d2._C[i] = -d2._R[i] * d2._t[i];
            }

            //-- Now the Xi :
            for (size_t i = 0; i < nbPoints; ++i)
            {
                size_t index = 3 * nViews;
                d2._X.col(i) = Vec3(vec_solution[index + i * 3], vec_solution[index + i * 3 + 1], vec_solution[index + i * 3 + 2]);
            }
        }

        // Compute residuals L2 from estimated parameter values :
        Vec2 xk, xsum(0.0, 0.0);
        for (size_t i = 0; i < d2._n; ++i)
        {
            for (size_t k = 0; k < d._x[0].cols(); ++k)
            {
                xk = project(d2.P(i), Vec3(d2._X.col(k)));
                xsum += Vec2((xk - d2._x[i].col(k)).array().pow(2));
            }
        }
        double dResidual2D = (xsum.array().sqrt().sum());

        // Check that 2D re-projection and 3D point are near to GT.
        BOOST_CHECK_SMALL(dResidual2D, 1e-4);
    }

    d2.exportToPLY("test_After_Infinity.ply");
}
#endif  // ALICEVISION_HAVE_MOSEK
