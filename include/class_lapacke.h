/*******************************************************************************
*
* Hamiltonian class definition
*
*******************************************************************************/

//--- Preprocessor directives --------------------------------------------------

#include <iostream>
#include <iomanip>
#include <cmath>
#include <complex>
#include <vector>

#include <lapacke.h>
#include <lambda_lanczos/lambda_lanczos.hpp>

using namespace std;
using lambda_lanczos::LambdaLanczos;

//--- Contents -----------------------------------------------------------------

#ifndef PARAM_CLASS_H
#define PARAM_CLASS_H

struct HamiltParameters {
    int pbc_flag=0, sparse_flag=0;
    int num_sites=2, num_states=4;
    double gz_field=0., gx_field=0., gy_field=0.;
    double hz_field=0., hx_field=0., hy_field=0.;
    vector<vector<int>> comp_basis;
};

#endif

#ifndef HAMILT_CLASS_L_H
#define HAMILT_CLASS_L_H

class hamilt_L {
    /* Hamiltonian operator class */
    /* Class constructor ******************************************

    Inputs:

    LENGHT = total number of sites.

    SPARSE_FLAG = condition on storing or not the Hamiltonian.
                  0 build the complete Hamiltonian.
                  else number of desired eigenvalues.

    PBC_FLAG = flag for the boundary conditions.
               0 open boundary conditions.
               else periodic boundary conditions.

    G_FIELD = amplitude of the spin coupling field

    H_FIELD = amplitude of the single spin field

    *************************************************************/
private:
    int spr_flag_, pbc_flag_;
    int tot_length_, tot_states_;
    double gx_field, gy_field, gz_field;
    double hx_field, hy_field, hz_field;
    vector<vector<int>> basis;

public:
    vector<double> eigval;
    vector<vector<complex<double>>> hamilt, eigvec;

    hamilt_L(const HamiltParameters& param):
        tot_states_(param.num_states),
        tot_length_(param.num_sites),
        spr_flag_(param.sparse_flag),
        pbc_flag_(param.pbc_flag),
        gx_field(param.gx_field),
        gy_field(param.gy_field),
        gz_field(param.gz_field),
        hx_field(param.hx_field),
        hy_field(param.hy_field),
        hz_field(param.hz_field),
        basis(param.comp_basis)
    {// CONSTRUCTION BEGIN

        if (spr_flag_) {
            // Initialize partial eigen-stuff
            eigval.resize(spr_flag_);
            eigvec.resize(spr_flag_, vector<complex<double>>(tot_states_, 0.));
        } else {
            // Initialize complete eigen-stuff
            eigval.resize(tot_states_);
            eigvec.resize(tot_states_, vector<complex<double>>(tot_states_, 0.));
            // Build the Hamiltonian
            hamilt.resize(tot_states_, vector<complex<double>>(tot_states_, 0.));
            buildHamilt();
        }

    }// CONSTRUCTION END

    void buildHamilt() {
        /* Subroutine to build the Hamiltonian */

        int index = 0;
        // Initialize Hamiltonian
        for (int i = 0; i < tot_states_; i++)
            for (int j = 0; j < tot_states_; j++)
                hamilt[i][j] = 0.;
        // Sigma_z [longitudinal field]
        for (int i = 0; i < tot_length_; i++) {
            for (int n = 0; n < tot_states_; n++) {
                if (basis[n][i] == 1) hamilt[n][n] -= hz_field;
                if (basis[n][i] == 0) hamilt[n][n] += hz_field;
            }
        }
        // Sigma_x [transverse field]
        for (int i = 0; i < tot_length_; i++) {
            for (int n = 0; n < tot_states_; n++) {
                if (basis[n][i] == 1) index = n - pow(2, tot_length_ - 1 - i);
                if (basis[n][i] == 0) index = n + pow(2, tot_length_ - 1 - i);
                hamilt[index][n] += hx_field;
            }
        }
        // Sigma_y [transverse field]
        for (int i = 0; i < tot_length_; i++) {
            for (int n = 0; n < tot_states_; n++) {
                if (basis[n][i] == 1) {
                    index = n - pow(2, tot_length_ - 1 - i);
                    hamilt[index][n] -= hy_field * complex(0., 1.);
                }
                if (basis[n][i] == 0) {
                    index = n + pow(2, tot_length_ - 1 - i);
                    hamilt[index][n] += hy_field * complex(0., 1.);
                }
            }
        }
        // Sigma_z Sigma_z [coupling]
        for (int i = 0; i < tot_length_ - 1; i++) {
            for (int n = 0; n < tot_states_; n++) {
                if (basis[n][i] == basis[n][i+1]) hamilt[n][n] += gz_field;
                if (basis[n][i] != basis[n][i+1]) hamilt[n][n] -= gz_field;
            }
        }
        // Sigma_z Sigma_z [boundary conditions]
        if (pbc_flag_) {
            for (int n = 0; n < tot_states_; n++) {
                if (basis[n][tot_length_ - 1] == basis[n][0])
                    hamilt[n][n] += gz_field;
                if (basis[n][tot_length_ - 1] != basis[n][0])
                    hamilt[n][n] -= gz_field;
            }
        }
        // Sigma_x Sigma_x [coupling]
        for (int i = 0; i < tot_length_ - 1; i++) {
            for (int n = 0; n < tot_states_; n++) {
                if (basis[n][i] == 1)
                    index = n - pow(2, tot_length_ - 1 - i);
                if (basis[n][i] == 0)
                    index = n + pow(2, tot_length_ - 1 - i);
                if (basis[n][i+1] == 1)
                    index -= pow(2, tot_length_ - 2 - i);
                if (basis[n][i+1] == 0)
                    index += pow(2, tot_length_ - 2 - i);
                hamilt[index][n] +=  gx_field;
            }
        }
        // Sigma_x Sigma_x [boundary conditions]
        if (pbc_flag_) {
            for (int n = 0; n < tot_states_; n++) {
                if (basis[n][0] == 1)
                    index = n - pow(2, tot_length_ - 1);
                if (basis[n][0] == 0)
                    index = n + pow(2, tot_length_ - 1);
                if (basis[n][tot_length_ - 1] == 1)
                    index -= 1;
                if (basis[n][tot_length_ - 1] == 0)
                    index += 1;
                hamilt[index][n] +=  gx_field;
            }
        }
        // Sigma_y Sigma_y [coupling]
        for (int i = 0; i < tot_length_ - 1; i++) {
            for (int n = 0; n < tot_states_; n++) {
                if (basis[n][i] == 1)
                    index = n - pow(2, tot_length_ - 1 - i);
                if (basis[n][i] == 0)
                    index = n + pow(2, tot_length_ - 1 - i);
                if (basis[n][i+1] == 1)
                    index -= pow(2, tot_length_ - 2 - i);
                if (basis[n][i+1] == 0)
                    index += pow(2, tot_length_ - 2 - i);

                if (basis[n][i] == basis[n][i+1])
                    hamilt[index][n] -= gy_field;
                if (basis[n][i] != basis[n][i+1])
                    hamilt[index][n] +=  gy_field;
            }
        }
        // Sigma_y Sigma_y [boundary conditions]
        if (pbc_flag_) {
            for (int n = 0; n < tot_states_; n++) {
                if (basis[n][0] == 1)
                    index = n - pow(2, tot_length_ - 1);
                if (basis[n][0] == 0)
                    index = n + pow(2, tot_length_ - 1);
                if (basis[n][tot_length_ - 1] == 1)
                    index -= 1;
                if (basis[n][tot_length_ - 1] == 0)
                    index += 1;

                if (basis[n][tot_length_ - 1] == basis[n][0])
                    hamilt[index][n] -=  gy_field;
                if (basis[n][tot_length_ - 1] != basis[n][0])
                    hamilt[index][n] +=  gy_field;
            }
        }
        // Check if the Hamiltonian is hermitian
        for (int i = 0; i < tot_states_; i++) {
            for (int j = 0; j < tot_states_; j++) {
                if (hamilt[i][j] != conj(hamilt[j][i])) {
                  cout << "Error occurred building the matrix: ";
                  cout << "it is not hermitian!" << endl;
                  show_hamiltonian();
                  exit(1);
                }
            }
        }

        cout << "Building complete!" << endl;
    }

    vector<complex<double>> action(const vector<complex<double>>& psi_in){
        /* Action of the Hamiltonian on a given input state */

        int index;
        vector<complex<double>> psi_out(tot_states_, 0.);

        // Sigma_z [longitudinal field]
        for (int i = 0; i < tot_length_; i++) {
            for (int n = 0; n < tot_states_; n++) {
                if (basis[n][i] == 1) psi_out[n] -= hz_field * psi_in[n];
                if (basis[n][i] == 0) psi_out[n] += hz_field * psi_in[n];
            }
        }
        // Sigma_x [transverse field]
        for (int i = 0; i < tot_length_; i++) {
            for (int n = 0; n < tot_states_; n++) {
                if (basis[n][i] == 1) index = n - pow(2, tot_length_ - 1 - i);
                if (basis[n][i] == 0) index = n + pow(2, tot_length_ - 1 - i);
                psi_out[index] += hx_field * psi_in[n];
            }
        }
        // Sigma_y [transverse field]
        for (int i = 0; i < tot_length_; i++) {
            for (int n = 0; n < tot_states_; n++) {
                if (basis[n][i] == 1) {
                    index = n - pow(2, tot_length_ - 1 - i);
                    psi_out[index] += hy_field * complex(0., -1.) * psi_in[n];
                }
                if (basis[n][i] == 0) {
                    index = n + pow(2, tot_length_ - 1 - i);
                    psi_out[index] += hy_field * complex(0., 1.) * psi_in[n];
                }
            }
        }
        // Sigma_z Sigma_z [coupling]
        for (int i = 0; i < tot_length_ - 1; i++) {
            for (int n = 0; n < tot_states_; n++) {
                if (basis[n][i] == basis[n][i + 1])
                    psi_out[n] += gz_field * psi_in[n];
                if (basis[n][i] != basis[n][i + 1])
                    psi_out[n] -= gz_field * psi_in[n];
            }
        }
        // Sigma_z Sigma_z [boundary conditions]
        if (pbc_flag_) {
            for (int n = 0; n < tot_states_; n++) {
                if (basis[n][tot_length_ - 1] == basis[n][0])
                    psi_out[n] += gz_field * psi_in[n];
                if (basis[n][tot_length_ - 1] != basis[n][0])
                    psi_out[n] -= gz_field * psi_in[n];
          }
        }
        // Sigma_x Sigma_x [coupling]
        for (int i = 0; i < tot_length_ - 1; i++) {
            for (int n = 0; n < tot_states_; n++) {
                if (basis[n][i] == 1)
                    index = n - pow(2, tot_length_ - 1 - i);
                if (basis[n][i] == 0)
                    index = n + pow(2, tot_length_ - 1 - i);
                if (basis[n][i+1] == 1)
                    index -= pow(2, tot_length_ - 2 - i);
                if (basis[n][i+1] == 0)
                    index += pow(2, tot_length_ - 2 - i);
                psi_out[index] += gx_field * psi_in[n];
            }
        }
        // Sigma_x Sigma_x [boundary conditions]
        if (pbc_flag_) {
            for (int n = 0; n < tot_states_; n++) {
                if (basis[n][0] == 1)
                    index = n - pow(2, tot_length_ - 1);
                if (basis[n][0] == 0)
                    index = n + pow(2, tot_length_ - 1);
                if (basis[n][tot_length_ - 1] == 1)
                    index -= 1;
                if (basis[n][tot_length_ - 1] == 0)
                    index += 1;
                psi_out[index] += gx_field * psi_in[n];
            }
        }
        // Sigma_y Sigma_y [coupling]
        for (int i = 0; i < tot_length_ - 1; i++) {
            for (int n = 0; n < tot_states_; n++) {
                if (basis[n][i] == 1)
                    index = n - pow(2, tot_length_ - 1 - i);
                if (basis[n][i] == 0)
                    index = n + pow(2, tot_length_ - 1 - i);
                if (basis[n][i+1] == 1)
                    index -= pow(2, tot_length_ - 2 - i);
                if (basis[n][i+1] == 0)
                    index += pow(2, tot_length_ - 2 - i);

                if (basis[n][i] == basis[n][i+1])
                    psi_out[index] -= gy_field * psi_in[n];
                if (basis[n][i] != basis[n][i+1])
                    psi_out[index] += gy_field * psi_in[n];
            }
        }
        // Sigma_y Sigma_y [boundary conditions]
        if (pbc_flag_) {
            for (int n = 0; n < tot_states_; n++) {
                if (basis[n][0] == 1)
                    index = n - pow(2, tot_length_ - 1);
                if (basis[n][0] == 0)
                    index = n + pow(2, tot_length_ - 1);
                if (basis[n][tot_length_ - 1] == 1)
                    index -= 1;
                if (basis[n][tot_length_ - 1] == 0)
                    index += 1;

                if (basis[n][tot_length_ - 1] == basis[n][0])
                    psi_out[index] -= gy_field * psi_in[n];
                if (basis[n][tot_length_ - 1] != basis[n][0])
                    psi_out[index] += gy_field * psi_in[n];
            }
        }

        return psi_out;
    }

    double average_energy(const vector<complex<double>>& psi_in){
        /* Braket of the Hamiltonian on a given input state */

        complex<double> energy = 0.;
        vector<complex<double>> psi_out(tot_states_, 0.);

        psi_out = action(psi_in);
        for(int n = 0; n < tot_states_; n++) {
            energy += conj(psi_in[n]) * psi_out[n];
        }

        return energy.real();
    }

    void diagonalize(){
        /* Diagonalization subroutine with LAPACK and Lambda Lanczos */

        if (!spr_flag_) {
            // Complete diagonalization with LAPACKE ---------------------------

            lapack_int info;
            lapack_int n = tot_states_;
            lapack_int lda = n; // leading dimension of the matrix
            double eigenval[n]; // eigenvalues vector for zheev
            double val_1, val_2;
            lapack_complex_double matr[lda*n]; // hamiltonian

            // Cast the Hamiltonian into a LAPACK object
            for (int i = 0; i < n; i++) {
                eigenval[i] = 0.;
                for (int j = 0; j < n; j++) {
                    val_1 = hamilt[i][j].real();
                    val_2 = hamilt[i][j].imag();
                    matr[i*lda + j] = lapack_make_complex_double(val_1, val_2);
                }
            }
            // Solve eigenproblem
            // https://www.netlib.org/lapack/explore-html/df/d9a/
            //group__complex16_h_eeigen_gaf23fb5b3ae38072ef4890ba
            //43d5cfea2.html#gaf23fb5b3ae38072ef4890ba43d5cfea2
            info = LAPACKE_zheev(LAPACK_ROW_MAJOR, 'V', 'L', n, matr, lda, eigenval);
            // Check for convergence
            if (info > 0) cerr << "Algorithm zheev failed to compute eigenvalues." << endl;
            // Retrieve results
            for (int i = 0; i < n; i++) {
                eigval[i] = eigenval[i];
                for (int j = 0; j < lda; j++) {
                    eigvec[i][j] = matr[j*lda + i];
                }
            }
            //------------------------------------------------------------------
        } else {
            // Partial diagonalization with Lanczos ----------------------------
            // Lambda Lanczos calculates the smallest or largest eigenvalue and
            // the corresponding eigenvector of a symmetric real matrix.
            // https://github.com/Dario-Maglio/lambda-lanczos/blob/master/src/samples/sample4_use_Eigen_library.cpp

            // Define matrix-vector multiplication routine
            auto amul = [&](const vector<double>& in, vector<double>& out) {
                vector<complex<double>> psi(in.size());
                for(int i = 0; i < in.size(); i++) psi[i] = complex(in[i]);
                psi = action(psi);
                for(int i = 0; i < in.size(); i++) out[i] = psi[i].real();
            };
            // Construct solver object and compute
            vector<vector<double>> eigvenctors;
            LambdaLanczos<double> solver(amul, tot_states_, false, spr_flag_);
            solver.run(eigval, eigvenctors);
            // Retrieve results
            for (int i = 0; i < spr_flag_; i++) {
                for (int n = 0; n < tot_states_; n++)
                    eigvec[i][n] = complex(eigvenctors[i][n]);
            }

            //------------------------------------------------------------------
        }
    }

    //--- Set and Get methods --------------------------------------------------

    void set_fields(const HamiltParameters& param, bool debug=0) {
        /* Rebuild the Hamiltonian changing the field values */

        // Print changes
        if (debug) cout << "New couplings are "<< endl << setprecision(4)
            << "gz: " << gz_field << " --> " << param.gz_field << endl
            << "gx: " << gx_field << " --> " << param.gx_field << endl
            << "gy: " << gy_field << " --> " << param.gy_field << endl
            << "hz: " << hz_field << " --> " << param.hz_field << endl
            << "hx: " << hx_field << " --> " << param.hx_field << endl
            << "hy: " << hy_field << " --> " << param.hy_field << endl << endl;
        // Save the new couplings
        gx_field = param.gx_field;
        gy_field = param.gy_field;
        gz_field = param.gz_field;
        hx_field = param.hx_field;
        hy_field = param.hy_field;
        hz_field = param.hz_field;
        // Re-construct the Hamiltonian
        if(!spr_flag_) buildHamilt();

    }

    double get_eigenvalue(int k){
        /* After Diagonalization returns the k-th eigenvalue */

        int limit = tot_states_;
        if(spr_flag_) limit = spr_flag_;

        if(k >= limit)
            cerr << "Index exceeds the amount of eigenvectors." << endl;

        return eigval[k];
    }

    vector<complex<double>> get_eigenvector(int k){
        /* After Diagonalization returns the k-th eigenvector */

        int limit = tot_states_;
        if(spr_flag_) limit = spr_flag_;

        if(k >= limit)
            cerr << "Index exceeds the amount of eigenvectors." << endl;

        return eigvec[k];
    }

    //--- Show methods ---------------------------------------------------------

    void show_comput_basis() {
        cout << "Total number of states: " << tot_states_ << endl;
        cout << "Lattice computational basis: " << endl;
        for (const auto& state : basis) {
            cout << "|";
            for (int i = 0; i < tot_length_; i++) cout << state[i];
            cout << ">  ";
        }
        cout << endl << endl;
    }

    void show_hamiltonian() {
        /* Print the Hamiltonian matrix */

        if (spr_flag_) {
            cout << "Not allowed to print sparse Hamiltonian!" << endl << endl;
        } else {
            cout << "Hamiltonian" << endl;
            for (const auto& row : hamilt) {
                for (const auto& element : row) {
                    cout << fixed << setprecision(2) << element << " ";
                }
                cout << endl;
            }
            cout << endl;
        }
    }

    void show_eigenvectors() {
        /* Print stored eigenvectors */

        cout << "Eigenvectors" << endl;
        for (const auto& row : eigvec) {
            for (const auto& element : row) {
                cout << fixed << setprecision(2) << element << " ";
            }
            cout << endl;
        }
        cout << endl;
    }

    void show_eigenvalues() {
        /* Print stored eigenvalues */

        cout << "Eigenvalues" << endl;
        for (const auto& val : eigval) cout << val << " ";
        cout << endl << endl;
    }

    void show_eigen() {
        /* Print stored eigenvalues and corresponding eigenvectors */

        for (int i = 0; i < eigval.size(); i++) {
            cout << "Eigenvalue " << i << " -> " << get_eigenvalue(i) << endl;
            for (const auto& element : eigvec[i]) {
                cout << fixed << setprecision(2) << element << " ";
            }
            cout << endl;
        }
        cout << endl;
    }

};

#endif
