// SURFace Estimator(SURFE) - Terms and Conditions of Use

// Unless otherwise noted, computer program source code of the SURFace
// Estimator(SURFE) is covered under Crown Copyright, Government of Canada, and
// is distributed under the MIT License.

// The Canada wordmark and related graphics associated with this distribution
// are protected under trademark law and copyright law.No permission is granted
// to use them outside the parameters of the Government of Canada's corporate
// identity program. For more information, see
// http://www.tbs-sct.gc.ca/fip-pcim/index-eng.asp

// Copyright title to all 3rd party software distributed with the SURFace
// Estimator(SURFE) is held by the respective copyright holders as noted in
// those files.Users are asked to read the 3rd Party Licenses referenced with
// those assets.

// MIT License

// Copyright(c) 2017 Government of Canada

// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files(the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and / or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions :

// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT
// NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
// PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
// HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
// ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
// WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

#include <algorithm>
#include <basis.h>
#include <lajaunie.h>
#include <math_methods.h>
#include <matrix_solver.h>
#include <vector>

#include <fstream>
#include <iomanip>
#include <iostream>

using namespace Surfe;
bool Lajaunie_Approach::_get_polynomial_matrix_block(MatrixXd &poly_matrix) {
    int n_ip = _n_increment_pair;
    int n_p = b_parameters.n_planar;
    int n_t = b_parameters.n_tangent;

    p_basis = create_polynomial_basis(m_parameters.polynomial_order);

    if ((int)poly_matrix.rows() != b_parameters.n_poly_terms) return false;

    // for Interface Increment Pair Constraints:
    for (int j = 0; j < (int)_increment_pairs->size(); j++) {
        p_basis->set_point(_increment_pairs->at(j)[0]);
        VectorXd b1 = p_basis->basis();
        p_basis->set_point(_increment_pairs->at(j)[1]);
        VectorXd b2 = p_basis->basis();
        if ((int)b1.rows() != b_parameters.n_poly_terms ||
            (int)b2.rows() != b_parameters.n_poly_terms)
            return false;
        for (int k = 0; k < (int)b1.rows(); k++)
            poly_matrix(k, j) = b1(k) - b2(k);
    }
    // for planar points ...
    for (int j = 0; j < n_p; j++) {
        p_basis->set_point(b_input.planar->at(j));
        VectorXd bx = p_basis->dx();
        VectorXd by = p_basis->dy();
        VectorXd bz = p_basis->dz();
        if ((int)bx.rows() != b_parameters.n_poly_terms ||
            (int)by.rows() != b_parameters.n_poly_terms ||
            (int)bz.rows() != b_parameters.n_poly_terms)
            return false;
        for (int k = 0; k < (int)bx.rows(); k++) {
            poly_matrix(k, n_ip + 3 *j) = bx(k);
            poly_matrix(k, n_ip + 3 *j + 1) = by(k);
            poly_matrix(k, n_ip + 3 *j + 2) = bz(k);
        }
    }
    // for tangent points ...
    for (int j = 0; j < n_t; j++) {
        p_basis->set_point(b_input.tangent->at(j));
        VectorXd bx = p_basis->dx();
        VectorXd by = p_basis->dy();
        VectorXd bz = p_basis->dz();
        if ((int)bx.rows() != b_parameters.n_poly_terms ||
            (int)by.rows() != b_parameters.n_poly_terms ||
            (int)bz.rows() != b_parameters.n_poly_terms)
            return false;
        for (int k = 0; k < (int)bx.rows(); k++) {
            poly_matrix(k, n_ip + 3 *n_p + j) =
                b_input.tangent->at(j).tx() * bx(k) +
                b_input.tangent->at(j).ty() * by(k) +
                b_input.tangent->at(j).tz() * bz(k);
        }
    }

    return true;
}

bool
Lajaunie_Approach::_insert_polynomial_matrix_blocks_in_interpolation_matrix(
    const MatrixXd &poly_matrix, MatrixXd &interpolation_matrix) {
    int n_ip = _n_increment_pair;
    int n_p = b_parameters.n_planar;
    int n_t = b_parameters.n_tangent;

    if ((n_ip + 3 * n_p + n_t + poly_matrix.rows()) >
            interpolation_matrix.rows() ||
        (n_ip + 3 * n_p + n_t + poly_matrix.rows()) >
            interpolation_matrix.cols())
        return false;
    // build polynomial blocks
    // | A PT |
    // | P 0  |
    // start with P
    for (int j = 0; j < (int)poly_matrix.rows(); j++) {
        for (int k = 0; k < (int)poly_matrix.cols(); k++) {
            interpolation_matrix(n_ip + 3 * n_p + n_t + j, k) =
                poly_matrix(j, k);
            interpolation_matrix(k, n_ip + 3 *n_p + n_t + j) =
                interpolation_matrix(n_ip + 3 * n_p + n_t + j, k);
        }
    }

    for (int j = 0; j < (int)poly_matrix.rows(); j++) {
        for (int k = 0; k < (int)poly_matrix.rows(); k++) {
            interpolation_matrix(n_ip + 3 * n_p + n_t + j,
                                 n_ip + 3 *n_p + n_t + k) = 0.0;
        }
    }

    return true;
}

bool Lajaunie_Approach::_get_increment_pairs() {
    // the interface increment pairs
    int _n_interface_pairs = 0;
    for (int j = 0; j < (int)b_input.interface_point_lists->size(); j++)
        _n_interface_pairs +=
            ((int)b_input.interface_point_lists->at(j).size() - 1);
    for (int j = 0; j < (int)b_input.interface_point_lists->size(); j++) {
        for (int k = 0;
             k < ((int)b_input.interface_point_lists->at(j).size() - 1); k++) {
            std::vector<Interface> interface_incr_p;
            interface_incr_p.push_back(b_input.interface_point_lists->at(j)[0]);
            interface_incr_p.push_back(
                b_input.interface_point_lists->at(j)[k + 1]);
            _increment_pairs->push_back(interface_incr_p);
        }
    }
    _n_increment_pair = _n_interface_pairs;

    return true;
}

Lajaunie_Approach::Lajaunie_Approach(const model_parameters &m_p,
                                     const Basic_input &basic_i) {
    // set GUI parameters and basic input (interface, planar, tangent) data
    // members to class
    m_parameters = m_p;
    b_input = basic_i;

    _increment_pairs = new std::vector<std::vector<Interface> >();

    _iteration = 0;
}

bool Lajaunie_Approach::get_method_parameters() {
    // # of constraints for each constraint type ...
    b_parameters.n_interface = (int)b_input.itrface->size();
    b_parameters.n_inequality =
        0;  // no support for inequality. if there is inequalities use the
            // stratigraphic horizon method
    b_parameters.n_planar = (int)b_input.planar->size();
    b_parameters.n_tangent = (int)b_input.tangent->size();
    // Total number of constraints ...
    b_parameters.n_constraints =
        _n_increment_pair + 3 * b_parameters.n_planar + b_parameters.n_tangent;
    // Total number of equality constraints
    if (m_parameters.use_restricted_range)
        b_parameters.restricted_range = true;
    else
        b_parameters.n_equality = _n_increment_pair +
                                  3 * b_parameters.n_planar +
                                  b_parameters.n_tangent;

    // polynomial parameters ...
    if (!b_parameters.restricted_range) {
        b_parameters.poly_term =
            true;  // NOTE: May want to have this as an option
                   // when using SPD functions
        b_parameters.modified_basis = false;
        b_parameters.problem_type = Parameter_Types::Linear;
    } else {
        b_parameters.poly_term = false;
        b_parameters.modified_basis = true;
        b_parameters.problem_type = Parameter_Types::Quadratic;
    }
    int m = m_parameters.polynomial_order + 1;
    b_parameters.n_poly_terms =
        (int)(m * (m + 1) * (m + 2) / 6) -
        1;  // minus 1 due to the nature of the indepentent pair constraints and
            // the vanishing of the constant term in the polynomial

    return true;
}

bool Lajaunie_Approach::process_input_data() {
    if ((int)b_input.itrface->size() == 0)
        return false;
    else {
        if (!b_input.get_interface_data()) return false;

        // For greedy algorithm this function can get called many times.
        // Need to take some care and make sure we are not appending
        // to existing _increment_pairs list. clear if necessary
        if ((int)_increment_pairs->size() != 0) _increment_pairs->clear();

        if (!_get_increment_pairs()) return false;
    }

    if (m_parameters.use_restricted_range) {
        for (int j = 0; j < (int)b_input.planar->size(); j++) {
            b_input.planar->at(j).setNormalBounds(
                m_parameters.angular_uncertainty,
                m_parameters.angular_uncertainty / 2);  // Need more ROBUST
                                                        // METHOD. Try large
                                                        // statistical sampling
            // from von Mises spherical distribution
            cout << " Planar[" << j << "] Bounds: " << endl;
            cout << "	nx: " << b_input.planar->at(j).nx_lower_bound()
                 << " <= " << b_input.planar->at(j).nx()
                 << " <= " << b_input.planar->at(j).nx_upper_bound() << endl;
            cout << "	ny: " << b_input.planar->at(j).ny_lower_bound()
                 << " <= " << b_input.planar->at(j).ny()
                 << " <= " << b_input.planar->at(j).ny_upper_bound() << endl;
            cout << "	nz: " << b_input.planar->at(j).nz_lower_bound()
                 << " <= " << b_input.planar->at(j).nz()
                 << " <= " << b_input.planar->at(j).nz_upper_bound() << endl;
        }
        for (int j = 0; j < (int)b_input.tangent->size(); j++) {
            b_input.tangent->at(j)
                .setAngleBounds(m_parameters.angular_uncertainty);
            cout << " Tangent[" << j << "] Bounds: " << endl;
            cout << "	" << b_input.tangent->at(j).angle_lower_bound()
                 << " <= " << b_input.tangent->at(j).inner_product_constraint()
                 << " <= " << b_input.tangent->at(j).angle_upper_bound()
                 << endl;
        }
    }

    return true;
}

bool Lajaunie_Approach::setup_system_solver() {
    if (b_parameters.restricted_range) {
        int n_c = b_parameters.n_constraints;

        VectorXd b(n_c);
        VectorXd r(n_c);
        get_inequality_values(b, r);

        MatrixXd interpolation_matrix(n_c, n_c);
        if (!get_interpolation_matrix(interpolation_matrix)) return false;

        MatrixXd inequality_matrix(n_c, n_c);
        inequality_matrix = interpolation_matrix;

        Quadratic_Predictor_Corrector_LOQO *qpc =
            new Quadratic_Predictor_Corrector_LOQO(interpolation_matrix,
                                                   inequality_matrix, b, r);
        if (!qpc->solve()) {
            error_msg.append(" LOQO Quadratic Solver failure.");
            return false;
        }
        solver = qpc;
    } else {
        int n = b_parameters.n_equality + b_parameters.n_poly_terms;
        VectorXd equality_values(n);
        get_equality_values(equality_values);
        MatrixXd interpolation_matrix(n, n);
        if (!get_interpolation_matrix(interpolation_matrix)) return false;
        Linear_LU_decomposition *llu =
            new Linear_LU_decomposition(interpolation_matrix, equality_values);
        if (!llu->solve()) {
            error_msg.append(" Linear Solver failure.");
            return false;
        }
        solver = llu;
    }

    if (!_update_interface_iso_values()) return false;

    return true;
}

bool Lajaunie_Approach::get_minimial_and_excluded_input(
    Basic_input &greedy_input, Basic_input &excluded_input) {
    // get minimial number of interface points from each interface
    // First find the horizon with the largest number of points, from this get 3
    // points that are well separated For the other horizons, get two points
    // that
    // are well separated

    b_input.get_interface_data();  // get required interface data

    // Find horizon with largest number of points -- ref_index
    int ref_index = -1;
    int largest_num_of_points = 0;
    for (int j = 0; j < (int)b_input.interface_point_lists->size(); j++) {
        int num_of_points = (int)b_input.interface_point_lists->at(j).size();
        if (num_of_points > largest_num_of_points) {
            largest_num_of_points = num_of_points;
            ref_index = j;
        }
    }
    // put all of the points from the horizon with the largest num of points in
    // a
    // Point vector for distance analysis routines
    std::vector<Point> dense_horizon(
        b_input.interface_point_lists->at(ref_index).begin(),
        b_input.interface_point_lists->at(ref_index).end());
    // find the two points that are furtherest from each other from this horizon
    int TwoIndexes[2];
    double largest_separation_distance = 0;
    if (Find_STL_Vector_Indices_FurtherestTwoPoints(dense_horizon, TwoIndexes))
        largest_separation_distance = distance_btw_pts(
            dense_horizon[TwoIndexes[0]],
            dense_horizon[TwoIndexes[1]]);  // get the distance btw these points
    else
        return false;

    int ThirdIndex =
        Find_STL_Vector_Index_ofPointClosestToOtherPointWithinDistance(
            dense_horizon[TwoIndexes[0]], dense_horizon,
            largest_separation_distance / 2);  // get the third point that is
                                               // well separated from the other
                                               // two points
    std::vector<int> dense_horizon_indices;
    dense_horizon_indices.push_back(TwoIndexes[0]);
    dense_horizon_indices.push_back(TwoIndexes[1]);
    dense_horizon_indices.push_back(ThirdIndex);

    std::vector<Point> cur_included_pts;  // temp container for holding minimial
                                          // well separated
    // interface points from many different horizons
    for (int j = 0; j < 3; j++)
        cur_included_pts.push_back(dense_horizon[dense_horizon_indices[j]]);

    for (int j = 0; j < (int)b_input.interface_point_lists->size(); j++) {
        if (j != ref_index)  // already processed horizon
                             // b_input.interface_point_lists[ref_index]
        {
            // find a point in these horizon that is the furthest from the
            // already
            // included points
            std::vector<Point> j_horizon(
                b_input.interface_point_lists->at(j).begin(),
                b_input.interface_point_lists->at(j).end());
            int index1 =
                furtherest_neighbour_index(j_horizon, cur_included_pts);
            cur_included_pts.push_back(j_horizon[index1]);
            // find a point that is furthest from the index1 point
            int index2 =
                furtherest_neighbour_index(j_horizon[index1], j_horizon);
            cur_included_pts.push_back(j_horizon[index2]);
        }
    }

    // fill the data structures greedy_input & excluded_input
    for (int j = 0; j < (int)b_input.itrface->size(); j++) {
        bool exclude_index = true;
        for (int k = 0; k < (int)cur_included_pts.size(); k++) {
            if (b_input.itrface->at(j).x() == cur_included_pts[k].x() &&
                b_input.itrface->at(j).y() == cur_included_pts[k].y() &&
                b_input.itrface->at(j).z() == cur_included_pts[k].z()) {
                exclude_index = false;
                greedy_input.itrface->push_back(b_input.itrface->at(j));
                break;
            }
        }
        if (exclude_index)
            excluded_input.itrface->push_back(b_input.itrface->at(j));
    }

    greedy_input.planar->push_back(b_input.planar->at(0));
    for (int j = 1; j < (int)b_input.planar->size(); j++)
        excluded_input.planar->push_back(b_input.planar->at(j));
    for (int j = 0; j < (int)b_input.tangent->size(); j++)
        excluded_input.tangent->push_back(b_input.tangent->at(j));

    return true;
}
bool Lajaunie_Approach::measure_residuals(Basic_input &input) {
    if (solver == NULL) return false;

#pragma omp parallel sections
    {
#pragma omp section
        {
            // interface points - These are a bit special in the way to compute
            // residuals b/c these points are used as increment constraints
            for (int j = 0; j < (int)input.itrface->size(); j++) {
                // what is the reference level
                double reference_level = input.itrface->at(j).level();
                // find a reference point in interface_test_points[] that has
                // the same
                // reference level
                double actual_level_value;
                for (int k = 0;
                     k < (int)this->b_input.interface_test_points->size();
                     k++) {
                    if (this->b_input.interface_test_points->at(k).level() ==
                        reference_level) {
                        eval_scalar_interpolant_at_point(
                            this->b_input.interface_test_points->at(k));
                        actual_level_value =
                            this->b_input.interface_test_points->at(k)
                                .scalar_field();  // this is the value it should
                                                  // be
                        break;
                    }
                }
                eval_scalar_interpolant_at_point(input.itrface->at(j));
                input.itrface->at(j).setResidual(abs(
                    input.itrface->at(j).scalar_field() - actual_level_value));
            }
        }
#pragma omp section
        {
            // planar points
            for (int j = 0; j < (int)input.planar->size(); j++) {
                eval_vector_interpolant_at_point(input.planar->at(j));
                double angle = 0.0;
                std::vector<double> v1;
                std::vector<double> v2;
                v1.push_back(input.planar->at(j).nx());
                v1.push_back(input.planar->at(j).ny());
                v1.push_back(input.planar->at(j).nz());
                v2.push_back(input.planar->at(j).nx_interp());
                v2.push_back(input.planar->at(j).ny_interp());
                v2.push_back(input.planar->at(j).nz_interp());
                Math_methods::angle_btw_2_vectors(v1, v2, angle);
                input.planar->at(j).setResidual(angle);
            }
        }
#pragma omp section
        {
            // tangent points
            for (int j = 0; j < (int)input.tangent->size(); j++) {
                eval_vector_interpolant_at_point(input.tangent->at(j));
                double angle = 0.0;
                std::vector<double> v1;
                std::vector<double> v2;
                v1.push_back(input.tangent->at(j).tx());
                v1.push_back(input.tangent->at(j).ty());
                v1.push_back(input.tangent->at(j).tz());
                v2.push_back(input.tangent->at(j).nx_interp());
                v2.push_back(input.tangent->at(j).ny_interp());
                v2.push_back(input.tangent->at(j).nz_interp());
                Math_methods::angle_btw_2_vectors<double>(v1, v2, angle);
                input.tangent->at(j).setResidual(angle);
            }
        }
    }

    return true;
}

bool Lajaunie_Approach::append_greedy_input(Basic_input &input) {
    // This function can most likely be promoted the the Greedy parent class
    // Below section can be a lot of computations - leverage parallelism
    std::vector<int> planar_indices_to_include;      // PLANAR Observations
    std::vector<int> tangent_indices_to_include;     // TANGENT Observations
    std::vector<int> interface_indices_to_include;   // INTERFACE Observations
    std::vector<int> inequality_indices_to_include;  // INEQUALITIES
                                                     // Observations
    // For the first iteration only consider adding additional planar
    // constraints
    // These additional constraints could force large changes in the scalar
    // field
    // which could consequently pass closely to interface points not yet
    // included

    // 	planar_indices_to_include =
    // Get_Planar_STL_Vector_Indices_With_Large_Residuals(input.planar,
    // m_parameters.angular_uncertainty, b_input.GetPlanarAvgNNDist());
    // if
    // (planar_indices_to_include.size() != 0 )
    // 	{
    // 		this->b_input.planar->push_back(input.planar->at(planar_indices_to_include[0]));
    // 		input.planar->erase(input.planar->begin() +
    // planar_indices_to_include[0]); 		return true;
    // 	}
    // 	interface_indices_to_include =
    // Get_Interface_STL_Vector_Indices_With_Large_Residuals(input.itrface,
    // m_parameters.interface_uncertainty, b_input.GetInterfaceAvgNNDist());
    // if
    // (interface_indices_to_include.size() != 0 )
    // 	{
    // 		this->b_input.itrface->push_back(input.itrface->at(interface_indices_to_include[0]));
    // 		input.itrface->erase(input.itrface->begin() +
    // interface_indices_to_include[0]); 		return true;
    // 	}
    if (_iteration == 0)
        planar_indices_to_include =
            Get_Planar_STL_Vector_Indices_With_Large_Residuals(
                input.planar, m_parameters.angular_uncertainty,
                b_input.GetPlanarAvgNNDist());
    else {
#pragma omp parallel sections
        {
#pragma omp section
            {
                // PLANAR Observations
                planar_indices_to_include =
                    Get_Planar_STL_Vector_Indices_With_Large_Residuals(
                        input.planar, m_parameters.angular_uncertainty,
                        b_input.GetPlanarAvgNNDist());
            }
#pragma omp section
            {
                // TANGENT Observations
                tangent_indices_to_include =
                    Get_Tangent_STL_Vector_Indices_With_Large_Residuals(
                        input.tangent, m_parameters.angular_uncertainty,
                        b_input.GetTangentAvgNNDist());
            }
#pragma omp section
            {
                // INTERFACE Observations
                interface_indices_to_include =
                    Get_Interface_STL_Vector_Indices_With_Large_Residuals(
                        input.itrface, m_parameters.interface_uncertainty,
                        b_input.GetInterfaceAvgNNDist());
            }
#pragma omp section
            {
                // INEQUALITIES Observations
                inequality_indices_to_include =
                    Get_Inequality_STL_Vector_Indices_With_Large_Residuals(
                        input.inequality, b_input.GetInequalityAvgNNDist());
            }
        }
    }

    int pI2i = (int)planar_indices_to_include.size();
    int tI2i = (int)tangent_indices_to_include.size();
    int itrI2i = (int)interface_indices_to_include.size();
    int ieI2i = (int)inequality_indices_to_include.size();

    // Add to current greedy input
    for (int j = 0; j < pI2i; j++)
        this->b_input.planar->push_back(
            input.planar->at(planar_indices_to_include[j]));
    for (int j = 0; j < tI2i; j++)
        this->b_input.tangent->push_back(
            input.tangent->at(tangent_indices_to_include[j]));
    for (int j = 0; j < itrI2i; j++)
        this->b_input.itrface->push_back(
            input.itrface->at(interface_indices_to_include[j]));
    for (int j = 0; j < ieI2i; j++)
        this->b_input.inequality->push_back(
            input.inequality->at(inequality_indices_to_include[j]));

    // Remove data from input so that residuals do not have to be measured at
    // locations where the constraints are supplied
    for (int j = 0; j < pI2i; j++) {
        input.planar->erase(input.planar->begin() +
                            planar_indices_to_include[j]);
        for (int k = j; k < pI2i; k++) planar_indices_to_include[k]--;
    }
    for (int j = 0; j < tI2i; j++) {
        input.tangent->erase(input.tangent->begin() +
                             tangent_indices_to_include[j]);
        for (int k = j; k < tI2i; k++) tangent_indices_to_include[k]--;
    }
    for (int j = 0; j < itrI2i; j++) {
        input.itrface->erase(input.itrface->begin() +
                             interface_indices_to_include[j]);
        for (int k = j; k < itrI2i; k++) interface_indices_to_include[k]--;
    }
    for (int j = 0; j < ieI2i; j++) {
        input.inequality->erase(input.inequality->begin() +
                                inequality_indices_to_include[j]);
        for (int k = j; k < ieI2i; k++) inequality_indices_to_include[k]--;
    }

    if (pI2i != 0 || tI2i != 0 || itrI2i != 0 || ieI2i != 0)
        return true;
    else
        return false;

    // return false;
}

bool Lajaunie_Approach::convert_modified_kernel_to_rbf_kernel() {
    // prep for linear prob...
    // set the constraints...
    // for Lajaunie and Stratigraphic Surface methods we don't update the
    // itrface[].level()'s we update the _increment_pairs[][] level()'s
    for (int j = 0; j < (int)_increment_pairs->size(); j++) {
        eval_scalar_interpolant_at_point(_increment_pairs->at(j)[0]);
        eval_scalar_interpolant_at_point(_increment_pairs->at(j)[1]);
        _increment_pairs->at(j)[0]
            .setLevel(_increment_pairs->at(j)[0].scalar_field());
        _increment_pairs->at(j)[1]
            .setLevel(_increment_pairs->at(j)[1].scalar_field());
    }

    for (int j = 0; j < b_input.planar->size(); j++) {
        eval_vector_interpolant_at_point(b_input.planar->at(j));
        // debug
        // 		cout<<" Planar["<<j<<"]: "<<endl;
        // 		cout<<"	Nx = "<<b_input.planar->at(j).nx()<<" Nx
        // interpolated =
        // "<<b_input.planar->at(j).nx_interp()<<endl; 		cout<<"	Ny =
        // "<<b_input.planar->at(j).ny()<<" Ny interpolated =
        // "<<b_input.planar->at(j).ny_interp()<<endl; 		cout<<"	Nz =
        // "<<b_input.planar->at(j).nz()<<" Nz interpolated =
        // "<<b_input.planar->at(j).nz_interp()<<endl;
        double normal[3] = {b_input.planar->at(j).nx_interp(),
                            b_input.planar->at(j).ny_interp(),
                            b_input.planar->at(j).nz_interp()};
        b_input.planar->at(j).setNormal(normal[0], normal[1], normal[2]);
    }
    for (int j = 0; j < b_input.tangent->size(); j++) {
        eval_vector_interpolant_at_point(b_input.tangent->at(j));
        // 		cout<<" Tangent["<<j<<"]: "<<endl;
        // 		cout<<"	Tx = "<<b_input.tangent->at(j).tx()<<" Nx
        // interpolated =
        // "<<b_input.tangent->at(j).nx_interp()<<endl; 		cout<<"	Ty
        // =
        // "<<b_input.tangent->at(j).ty()<<" Ny interpolated =
        // "<<b_input.tangent->at(j).ny_interp()<<endl; 		cout<<"	Tz
        // =
        // "<<b_input.tangent->at(j).tz()<<" Nz interpolated =
        // "<<b_input.tangent->at(j).nz_interp()<<endl; 		cout<<" Tx*nx
        // + Ty*ny +
        // Tz*nz =
        // "<<b_input.tangent->at(j).tx()*b_input.tangent->at(j).nx_interp()
        // + b_input.tangent->at(j).ty()*b_input.tangent->at(j).ny_interp() +
        // 			b_input.tangent->at(j).tz()*b_input.tangent->at(j).nz_interp()<<endl;
        double vf[3] = {b_input.tangent->at(j).nx_interp(),
                        b_input.tangent->at(j).ny_interp(),
                        b_input.tangent->at(j).nz_interp()};
        double inner_product = vf[0] * b_input.tangent->at(j).tx() +
                               vf[1] * b_input.tangent->at(j).ty() +
                               vf[2] * b_input.tangent->at(j).tz();
        b_input.tangent->at(j).setInnerProductConstraint(inner_product);
    }

    // switch from modified kernel to normal rbf kernel
    kernel = rbf_kernel;

    if (m_parameters.use_restricted_range)
        b_parameters.restricted_range = false;
    b_parameters.n_equality =
        _n_increment_pair + 3 * b_parameters.n_planar + b_parameters.n_tangent;
    b_parameters.poly_term = true;
    b_parameters.n_poly_terms = 3;
    b_parameters.modified_basis = false;
    b_parameters.problem_type = Parameter_Types::Linear;
    int n_e = b_parameters.n_equality;
    int n_p = b_parameters.n_poly_terms;
    VectorXd equality_values(n_e + n_p);
    get_equality_values(equality_values);

    MatrixXd interpolation_matrix(n_e + n_p, n_e + n_p);
    if (!get_interpolation_matrix(interpolation_matrix)) return false;

    Linear_LU_decomposition *llu =
        new Linear_LU_decomposition(interpolation_matrix, equality_values);
    if (!llu->solve()) return false;
    solver = llu;

    if (!_update_interface_iso_values()) return false;

    return true;
}

bool Lajaunie_Approach::get_equality_values(VectorXd &equality_values) {
    int j = 0;
    int k = 0;
    int l = 0;
    int m = 0;
    for (j = 0; j < _n_increment_pair; j++)
        equality_values(j) = _increment_pairs->at(j)[0].level() -
                             _increment_pairs->at(j)[1].level();
    ;
    for (k = 0; k < (int)b_input.planar->size(); k++) {
        equality_values(3 *k + j) = b_input.planar->at(k).nx();
        equality_values(3 *k + j + 1) = b_input.planar->at(k).ny();
        equality_values(3 *k + j + 2) = b_input.planar->at(k).nz();
    }
    for (l = 0; l < (int)b_input.tangent->size(); l++)
        equality_values(l + 3 *k + j) = 0.0;
    if (b_parameters.poly_term)
        for (m = 0; m < (int)b_parameters.n_poly_terms; m++)
            equality_values(m + l + 3 *k + j) = 0.0;

    return true;
}

bool Lajaunie_Approach::get_inequality_values(VectorXd &b, VectorXd &r) {
    int n_ip = _n_increment_pair;
    int n_p = b_parameters.n_planar;
    int n_t = b_parameters.n_tangent;

    // interface data
    for (int j = 0; j < n_ip; j++) {
        b(j) = -m_parameters.interface_uncertainty;
        r(j) = 2 * m_parameters.interface_uncertainty;
    }

    // planar data
    for (int j = 0; j < n_p; j++) {
        // x-component
        b(n_ip + 3 *j + 0) = b_input.planar->at(j).nx_lower_bound();
        r(n_ip + 3 *j + 0) = b_input.planar->at(j).nx_upper_bound() -
                             b_input.planar->at(j).nx_lower_bound();
        // y-component
        b(n_ip + 3 *j + 1) = b_input.planar->at(j).ny_lower_bound();
        r(n_ip + 3 *j + 1) = b_input.planar->at(j).ny_upper_bound() -
                             b_input.planar->at(j).ny_lower_bound();
        // z-component
        b(n_ip + 3 *j + 2) = b_input.planar->at(j).nz_lower_bound();
        r(n_ip + 3 *j + 2) = b_input.planar->at(j).nz_upper_bound() -
                             b_input.planar->at(j).nz_lower_bound();
    }

    // tangent data
    for (int j = 0; j < n_t; j++) {
        b(n_ip + 3 *n_p + j) = b_input.tangent->at(j).angle_lower_bound();
        r(n_ip + 3 *n_p + j) = b_input.tangent->at(j).angle_upper_bound() -
                               b_input.tangent->at(j).angle_lower_bound();
    }

    return true;
}

bool Lajaunie_Approach::get_interpolation_matrix(
    MatrixXd &interpolation_matrix) {
    int n_ip = _n_increment_pair;
    int n_p = b_parameters.n_planar;
    int n_t = b_parameters.n_tangent;

    // Row and Column constraint order : interface increment pair (iip) ->
    // planar
    // (p_x,p_y,p_z) -> tangent (t)

    // Base Matrix Structure
    // | iip/iip iip/p_y iip/p_y iip/p_z iip/t |
    // | p_x/iip p_x/p_x p_x/p_y p_x/p_z p_x/t |
    // | p_y/iip p_y/p_x p_y/p_y p_y/p_z p_y/t |
    // | p_z/iip p_z/p_x p_z/p_y p_z/p_z p_z/t |
    // |   t/iip  t/p_x   t/p_y   t/p_z   t/t |

    // Interface Increment Pair Constraints:
    for (int j = 0; j < (int)_increment_pairs->size(); j++) {
        // Row:interface increment pair/Column:interface increment pair block
        for (int k = 0; k < (int)_increment_pairs->size(); k++) {
            kernel->set_points(_increment_pairs->at(j)[0],
                               _increment_pairs->at(k)[0]);
            double v1 = kernel->basis_pt_pt();
            kernel->set_points(_increment_pairs->at(j)[0],
                               _increment_pairs->at(k)[1]);
            double v2 = kernel->basis_pt_pt();
            kernel->set_points(_increment_pairs->at(j)[1],
                               _increment_pairs->at(k)[0]);
            double v3 = kernel->basis_pt_pt();
            kernel->set_points(_increment_pairs->at(j)[1],
                               _increment_pairs->at(k)[1]);
            double v4 = kernel->basis_pt_pt();
            interpolation_matrix(j, k) = (v1 - v2) - (v3 - v4);
        }
        // Row:interface increment pair/Column:planar block
        for (int k = 0; k < n_p; k++) {
            kernel->set_points(_increment_pairs->at(j)[0],
                               b_input.planar->at(k));
            double v1x = kernel->basis_pt_planar_x();
            double v1y = kernel->basis_pt_planar_y();
            double v1z = kernel->basis_pt_planar_z();
            kernel->set_points(_increment_pairs->at(j)[1],
                               b_input.planar->at(k));
            double v2x = kernel->basis_pt_planar_x();
            double v2y = kernel->basis_pt_planar_y();
            double v2z = kernel->basis_pt_planar_z();
            interpolation_matrix(j, 3 *k + n_ip) = v1x - v2x;
            interpolation_matrix(j, 3 *k + n_ip + 1) = v1y - v2y;
            interpolation_matrix(j, 3 *k + n_ip + 2) = v1z - v2z;
        }
        // Row:interface increment pair/Column:tangent block
        for (int k = 0; k < n_t; k++) {
            kernel->set_points(_increment_pairs->at(j)[0],
                               b_input.tangent->at(k));
            double v1 = kernel->basis_pt_tangent();
            kernel->set_points(_increment_pairs->at(j)[1],
                               b_input.tangent->at(k));
            double v2 = kernel->basis_pt_tangent();
            interpolation_matrix(j, n_ip + 3 *n_p + k) = v1 - v2;
        }
    }
    // Planar Constraints
    for (int j = 0; j < n_p; j++) {
        // Row:planar/Column:interface increment pair
        for (int k = 0; k < (int)_increment_pairs->size(); k++) {
            kernel->set_points(b_input.planar->at(j),
                               _increment_pairs->at(k)[0]);
            double v1x = kernel->basis_planar_x_pt();
            double v1y = kernel->basis_planar_y_pt();
            double v1z = kernel->basis_planar_z_pt();
            kernel->set_points(b_input.planar->at(j),
                               _increment_pairs->at(k)[1]);
            double v2x = kernel->basis_planar_x_pt();
            double v2y = kernel->basis_planar_y_pt();
            double v2z = kernel->basis_planar_z_pt();
            interpolation_matrix(3 * j + n_ip, k) = v1x - v2x;
            interpolation_matrix(3 * j + n_ip + 1, k) = v1y - v2y;
            interpolation_matrix(3 * j + n_ip + 2, k) = v1z - v2z;
        }
        // Row:planar/Column:planar block
        for (int k = 0; k < n_p; k++) {
            kernel->set_points(b_input.planar->at(j), b_input.planar->at(k));
            interpolation_matrix(3 * j + n_ip, 3 *k + n_ip) =
                kernel->basis_planar_planar(Parameter_Types::DXDX);
            interpolation_matrix(3 * j + n_ip, 3 *k + n_ip + 1) =
                kernel->basis_planar_planar(Parameter_Types::DXDY);
            interpolation_matrix(3 * j + n_ip, 3 *k + n_ip + 2) =
                kernel->basis_planar_planar(Parameter_Types::DXDZ);
            interpolation_matrix(3 * j + n_ip + 1, 3 *k + n_ip) =
                kernel->basis_planar_planar(Parameter_Types::DYDX);
            interpolation_matrix(3 * j + n_ip + 1, 3 *k + n_ip + 1) =
                kernel->basis_planar_planar(Parameter_Types::DYDY);
            interpolation_matrix(3 * j + n_ip + 1, 3 *k + n_ip + 2) =
                kernel->basis_planar_planar(Parameter_Types::DYDZ);
            interpolation_matrix(3 * j + n_ip + 2, 3 *k + n_ip) =
                kernel->basis_planar_planar(Parameter_Types::DZDX);
            interpolation_matrix(3 * j + n_ip + 2, 3 *k + n_ip + 1) =
                kernel->basis_planar_planar(Parameter_Types::DZDY);
            interpolation_matrix(3 * j + n_ip + 2, 3 *k + n_ip + 2) =
                kernel->basis_planar_planar(Parameter_Types::DZDZ);
        }
        // Row:planar/Column:tangent block
        for (int k = 0; k < n_t; k++) {
            kernel->set_points(b_input.planar->at(j), b_input.tangent->at(k));
            interpolation_matrix(3 * j + n_ip, n_ip + 3 *n_p + k) =
                kernel->basis_planar_tangent(Parameter_Types::DX);
            interpolation_matrix(3 * j + n_ip + 1, n_ip + 3 *n_p + k) =
                kernel->basis_planar_tangent(Parameter_Types::DY);
            interpolation_matrix(3 * j + n_ip + 2, n_ip + 3 *n_p + k) =
                kernel->basis_planar_tangent(Parameter_Types::DZ);
        }
    }
    // Tangent Constraints
    for (int j = 0; j < n_t; j++) {
        // Row:tangent/Column:interface increment pair block
        for (int k = 0; k < (int)_increment_pairs->size(); k++) {
            kernel->set_points(b_input.tangent->at(j),
                               _increment_pairs->at(k)[0]);
            double v1 = kernel->basis_tangent_pt();
            kernel->set_points(b_input.tangent->at(j),
                               _increment_pairs->at(k)[1]);
            double v2 = kernel->basis_tangent_pt();
            interpolation_matrix(j + n_ip + 3 * n_p, k) = v1 - v2;
        }
        // Row:tangent/Column:planar block
        for (int k = 0; k < n_p; k++) {
            kernel->set_points(b_input.tangent->at(j), b_input.planar->at(k));
            interpolation_matrix(j + n_ip + 3 * n_p, 3 *k + n_ip) =
                kernel->basis_tangent_planar(Parameter_Types::DX);
            interpolation_matrix(j + n_ip + 3 * n_p, 3 *k + n_ip + 1) =
                kernel->basis_tangent_planar(Parameter_Types::DY);
            interpolation_matrix(j + n_ip + 3 * n_p, 3 *k + n_ip + 2) =
                kernel->basis_tangent_planar(Parameter_Types::DZ);
        }
        // Row:tangent/Column:tangent block
        for (int k = 0; k < n_t; k++) {
            kernel->set_points(b_input.tangent->at(j), b_input.tangent->at(k));
            interpolation_matrix(j + n_ip + 3 * n_p, n_ip + 3 *n_p + k) =
                kernel->basis_tangent_tangent();
        }
    }

    // build polynomial blocks if required
    // | A PT |
    // | P 0  |
    if (b_parameters.poly_term) {
        MatrixXd poly_matrix(b_parameters.n_poly_terms,
                             b_parameters.n_constraints);
        if (!_get_polynomial_matrix_block(poly_matrix)) return false;
        // fill remaining matrix blocks (P, PT, 0)
        if (!_insert_polynomial_matrix_blocks_in_interpolation_matrix(
                 poly_matrix, interpolation_matrix))
            return false;
    }

    return true;
}

Polynomial_Basis *Lajaunie_Approach::create_polynomial_basis(
    const int &poly_order) {
    if (poly_order == 0)
        return new Poly_Zero(true);
    else if (poly_order == 1)
        return new Poly_First(true);
    else
        return new Poly_Second(true);
}

void Lajaunie_Approach::eval_scalar_interpolant_at_point(Point &p) {
    int n_ip = _n_increment_pair;
    int n_p = b_parameters.n_planar;
    int n_t = b_parameters.n_tangent;

    Kernel *kernel_j = kernel->clone();
    double elemsum_1 = 0.0;
    double elemsum_2 = 0.0;
    double elemsum_3 = 0.0;
    double poly = 0.0;
    for (int k = 0; k < (int)_increment_pairs->size(); k++) {
        kernel_j->set_points(p, _increment_pairs->at(k)[0]);
        double v1 = kernel_j->basis_pt_pt();
        kernel_j->set_points(p, _increment_pairs->at(k)[1]);
        double v2 = kernel_j->basis_pt_pt();
        elemsum_1 += solver->weights[k] * (v1 - v2);
    }
    for (int k = 0; k < n_p; k++) {
        kernel_j->set_points(p, b_input.planar->at(k));
        elemsum_2 +=
            solver->weights[n_ip + 3 * k] * kernel_j->basis_pt_planar_x();
        elemsum_2 +=
            solver->weights[n_ip + 3 * k + 1] * kernel_j->basis_pt_planar_y();
        elemsum_2 +=
            solver->weights[n_ip + 3 * k + 2] * kernel_j->basis_pt_planar_z();
    }
    for (int k = 0; k < n_t; k++) {
        kernel_j->set_points(p, b_input.tangent->at(k));
        elemsum_3 +=
            solver->weights[n_ip + 3 * n_p + k] * kernel_j->basis_pt_tangent();
    }
    if (b_parameters.poly_term) {
        Polynomial_Basis *p_basis_j = p_basis->clone();
        p_basis_j->set_point(p);
        VectorXd b = p_basis_j->basis();
        for (int k = 0; k < (int)b.size(); k++)
            poly += b(k) * solver->weights[n_ip + 3 * n_p + n_t + k];
        delete p_basis_j;
    }
    p.set_scalar_field(elemsum_1 + elemsum_2 + elemsum_3 + poly);
    delete kernel_j;
}

void Lajaunie_Approach::eval_vector_interpolant_at_point(Point &p) {
    int n_ip = _n_increment_pair;
    int n_p = b_parameters.n_planar;
    int n_t = b_parameters.n_tangent;

    Kernel *kernel_j = kernel->clone();
    double elemsum_1_x = 0.0;
    double elemsum_1_y = 0.0;
    double elemsum_1_z = 0.0;
    double elemsum_2_x = 0.0;
    double elemsum_2_y = 0.0;
    double elemsum_2_z = 0.0;
    double elemsum_3_x = 0.0;
    double elemsum_3_y = 0.0;
    double elemsum_3_z = 0.0;
    double poly_x = 0.0;
    double poly_y = 0.0;
    double poly_z = 0.0;
    // interface constraints
    for (int k = 0; k < n_ip; k++) {
        kernel->set_points(p, _increment_pairs->at(k)[0]);
        double v1x = kernel->basis_planar_x_pt();
        double v1y = kernel->basis_planar_y_pt();
        double v1z = kernel->basis_planar_z_pt();
        kernel->set_points(p, _increment_pairs->at(k)[1]);
        double v2x = kernel->basis_planar_x_pt();
        double v2y = kernel->basis_planar_y_pt();
        double v2z = kernel->basis_planar_z_pt();
        elemsum_1_x += solver->weights[k] * (v1x - v2x);
        elemsum_1_y += solver->weights[k] * (v1y - v2y);
        elemsum_1_z += solver->weights[k] * (v1z - v2z);
    }
    // planar constraints
    for (int k = 0; k < n_p; k++) {
        kernel->set_points(p, b_input.planar->at(k));
        elemsum_2_x += solver->weights[n_ip + 0 + 3 * k] *
                       kernel->basis_planar_planar(Parameter_Types::DXDX);
        elemsum_2_x += solver->weights[n_ip + 1 + 3 * k] *
                       kernel->basis_planar_planar(Parameter_Types::DXDY);
        elemsum_2_x += solver->weights[n_ip + 2 + 3 * k] *
                       kernel->basis_planar_planar(Parameter_Types::DXDZ);
        elemsum_2_y += solver->weights[n_ip + 0 + 3 * k] *
                       kernel->basis_planar_planar(Parameter_Types::DYDX);
        elemsum_2_y += solver->weights[n_ip + 1 + 3 * k] *
                       kernel->basis_planar_planar(Parameter_Types::DYDY);
        elemsum_2_y += solver->weights[n_ip + 2 + 3 * k] *
                       kernel->basis_planar_planar(Parameter_Types::DYDZ);
        elemsum_2_z += solver->weights[n_ip + 0 + 3 * k] *
                       kernel->basis_planar_planar(Parameter_Types::DZDX);
        elemsum_2_z += solver->weights[n_ip + 1 + 3 * k] *
                       kernel->basis_planar_planar(Parameter_Types::DZDY);
        elemsum_2_z += solver->weights[n_ip + 2 + 3 * k] *
                       kernel->basis_planar_planar(Parameter_Types::DZDZ);
    }
    // Tangent constraints
    for (int k = 0; k < n_t; k++) {
        kernel->set_points(p, b_input.tangent->at(k));
        elemsum_3_x += solver->weights[n_ip + 3 * n_p + k] *
                       kernel->basis_planar_tangent(Parameter_Types::DX);
        elemsum_3_y += solver->weights[n_ip + 3 * n_p + k] *
                       kernel->basis_planar_tangent(Parameter_Types::DY);
        elemsum_3_z += solver->weights[n_ip + 3 * n_p + k] *
                       kernel->basis_planar_tangent(Parameter_Types::DZ);
    }
    if (b_parameters.poly_term) {
        Polynomial_Basis *p_basis_j = p_basis->clone();
        p_basis_j->set_point(p);
        VectorXd bx = p_basis_j->dx();
        VectorXd by = p_basis_j->dy();
        VectorXd bz = p_basis_j->dz();
        for (int k = 0; k < (int)bx.size(); k++) {
            poly_x += bx(k) * solver->weights[n_ip + 3 * n_p + n_t + k];
            poly_y += by(k) * solver->weights[n_ip + 3 * n_p + n_t + k];
            poly_z += bz(k) * solver->weights[n_ip + 3 * n_p + n_t + k];
        }
        delete p_basis_j;
    }
    double nx = elemsum_1_x + elemsum_2_x + elemsum_3_x + poly_x;
    double ny = elemsum_1_y + elemsum_2_y + elemsum_3_y + poly_y;
    double nz = elemsum_1_z + elemsum_2_z + elemsum_3_z + poly_z;
    p.set_vector_field(nx, ny, nz);
    delete kernel_j;
}
