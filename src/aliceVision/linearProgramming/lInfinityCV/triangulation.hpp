// This file is part of the AliceVision project.
// Copyright (c) 2016 AliceVision contributors.
// Copyright (c) 2012 openMVG contributors.
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file,
// You can obtain one at https://mozilla.org/MPL/2.0/.

#ifndef ALICEVISION_LINFINITY_COMPUTER_VISION_TRIANGULATION_H_
#define ALICEVISION_LINFINITY_COMPUTER_VISION_TRIANGULATION_H_

#include "aliceVision/numeric/numeric.hpp"
#include "aliceVision/linearProgramming/ISolver.hpp"
#include <utility>
#include <vector>

//--
//- Implementation of algorithm from Paper titled :
//- [1] "Multiple-View Geometry under the L_\infty Norm."
//- Author : Fredrik Kahl, Richard Hartley.
//- Date : 9 sept 2008.

//- [2] "Multiple View Geometry and the L_\infty -norm"
//- Author : Fredrik Kahl
//- ICCV 2005.
//--

namespace aliceVision {
namespace lInfinityCV {

using namespace linearProgramming;

//-- Triangulation
//    - Estimation of X from Pi and xij
// [1] -> 5.1 The triangulation problem
//
//    - This implementation Use L1 norm instead of the L2 norm of
//      the paper, it allows to use standard standard LP
//      (simplex) instead of using SOCP (second order cone programming).
//      Implementation by Pierre Moulon
//

inline void EncodeTriangulation(const std::vector<Mat34>& Pi,  // Projection matrices
                                const Mat2X& x_ij,             // corresponding observations
                                double gamma,                  // Start upper bound
                                Mat& A,
                                Vec& C)
{
    // Build A, C matrix.

    const size_t nbCamera = Pi.size();
    A.resize(5 * nbCamera, 3);
    C.resize(5 * nbCamera, 1);

    int cpt = 0;
    for (int i = 0; i < nbCamera; ++i)
    {
        const Mat3 R = Pi[i].block<3, 3>(0, 0);
        const Vec3 t = Pi[i].block<3, 1>(0, 3);
        const Mat2X pt = x_ij.col(i);

        // A (Rotational part):
        A.block<1, 3>(cpt, 0) = R.row(0) - pt(0) * R.row(2) - gamma * R.row(2);
        A.block<1, 3>(cpt + 1, 0) = R.row(1) - pt(1) * R.row(2) - gamma * R.row(2);
        A.block<1, 3>(cpt + 2, 0) = -R.row(2);
        A.block<1, 3>(cpt + 3, 0) = -R.row(0) + pt(0) * R.row(2) - gamma * R.row(2);
        A.block<1, 3>(cpt + 4, 0) = -R.row(1) + pt(1) * R.row(2) - gamma * R.row(2);

        // C (translation part):
        C(cpt) = -t(0) + pt(0) * t(2) + gamma * t(2);
        C(cpt + 1) = -t(1) + pt(1) * t(2) + gamma * t(2);
        C(cpt + 2) = t(2);
        C(cpt + 3) = t(0) - pt(0) * t(2) + gamma * t(2);
        C(cpt + 4) = t(1) - pt(1) * t(2) + gamma * t(2);

        //- Next entry
        cpt += 5;
    }
}

/// Kernel that set Linear constraints for the Triangulation Problem.
///  Designed to be used with bisectionLP and ISolver interface.
///
/// Triangulation :
///    - Estimation of Xi from Pj and xij
/// Implementation of problem of [1] -> 5.1 The triangulation problem
///  under a Linear Program form.
struct Triangulation_L1_ConstraintBuilder
{
    Triangulation_L1_ConstraintBuilder(const std::vector<Mat34>& vec_Pi, const Mat2X& x_ij)
    {
        _vec_Pi = vec_Pi;
        _x_ij = x_ij;
    }

    /// Setup constraints of the triangulation problem as a Linear program
    bool Build(double gamma, LPConstraints& constraint)
    {
        EncodeTriangulation(_vec_Pi, _x_ij, gamma, constraint._constraintMat, constraint._Cst_objective);
        //-- Setup additional information about the Linear Program constraint
        // We look for 3 variables [X,Y,Z] with no bounds.
        // Constraint sign are all less or equal (<=)
        constraint._nbParams = 3;
        constraint._vec_bounds = std::vector<std::pair<double, double>>(1);
        fill(constraint._vec_bounds.begin(), constraint._vec_bounds.end(), std::make_pair((double)-1e+30, (double)1e+30));
        // Setup constraint sign
        constraint._vec_sign.resize(constraint._constraintMat.rows());
        fill(constraint._vec_sign.begin(), constraint._vec_sign.end(), LPConstraints::LP_LESS_OR_EQUAL);

        return true;
    }

    //-- Data required for triangulation :
    std::vector<Mat34> _vec_Pi;  // Projection matrix
    Mat2X _x_ij;                 // 2d projection : xij = Pj*Xi
};

}  // namespace lInfinityCV
}  // namespace aliceVision

#endif  // ALICEVISION_LINFINITY_COMPUTER_VISION_TRIANGULATION_H_
