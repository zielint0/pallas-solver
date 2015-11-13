/*!
* \file basinhopping.h
*
* \author Ryan Latture
* \date 5-18-15
*
* This file contains a C++ implementation of the basinhopping algorithm.
* This code relies on the Google Ceres local minimization functions, and
* is inspired by the SciPy implementaion found in scipy.optimize
*/

// Pallas Solver
// Copyright 2015. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// * Redistributions of source code must retain the above copyright notice,
//   this list of conditions and the following disclaimer.
// * Redistributions in binary form must reproduce the above copyright notice,
//   this list of conditions and the following disclaimer in the documentation
//   and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//
// Author: ryan.latture@gmail.com (Ryan Latture)

#ifndef PALLAS_BASINHOPPING_H
#define PALLAS_BASINHOPPING_H

#include <cfloat>

#include "pallas/scoped_ptr.h"
#include "pallas/step_function.h"
#include "pallas/types.h"
#include "pallas/internal/metropolis.h"
#include "pallas/internal/state.h"


namespace pallas {

    /**
     * @brief Minimizes an objective function by sequentially hopping between minima in the objective's energy landscape.
     * @details Basin-hopping is a stochastic algorithm which attempts to find the global minimum of a smooth scalar function 
     * of one or more variables. The algorithm in its current form was described by David Wales and Jonathan Doye http://www-wales.ch.cam.ac.uk/.\n

      The algorithm is iterative with each cycle composed of the following features\n

        1. random perturbation of the coordinates
        2. local minimization
        3. accept or reject the new coordinates based on the minimized function value

      The acceptance test used here is the Metropolis criterion of standard Monte Carlo algorithms.\n \n
      <B>Example</B>
     * @code
     #include "glog/logging.h"

    // Each solver is defined in its own header file.
    // include the solver you wish you use:
    #include "pallas/basinhopping.h"

    // define a problem you wish to solve by inheriting
    // from the pallas::GradientCostFunction interface
    // and implementing the Evaluate and NumParameters methods.
    class Rosenbrock : public pallas::GradientCostFunction {
    public:
        virtual ~Rosenbrock() {}

        virtual bool Evaluate(const double* parameters,
                              double* cost,
                              double* gradient) const {
            const double x = parameters[0];
            const double y = parameters[1];

            cost[0] = (1.0 - x) * (1.0 - x) + 100.0 * (y - x * x) * (y - x * x);
            if (gradient != NULL) {
                gradient[0] = -2.0 * (1.0 - x) - 200.0 * (y - x * x) * 2.0 * x;
                gradient[1] = 200.0 * (y - x * x);
            }
            return true;
        }

        virtual int NumParameters() const { return 2; }
    };

    int main(int argc, char** argv) {
        google::InitGoogleLogging(argv[0]);

        // define the starting point for the optimization
        double parameters[2] = {-1.2, 0.0};

        // set up global optimizer options only initialization
        // is need to accept the default options
        pallas::Basinhopping::Options options;

        // initialize a summary object to hold the
        // optimization details
        pallas::Basinhopping::Summary summary;

        // create a problem from your cost function
        pallas::GradientProblem problem(new Rosenbrock());

        // solve the problem and store the optimal position
        // in parameters and the optimization details in
        // the summary
        pallas::Solve(options, problem, parameters, &summary);

        std::cout << summary.FullReport() << std::endl;
        std::cout << "Global minimum found at:" << std::endl;
        std::cout << "\tx: " << parameters[0] << "\ty: " << parameters[1] << std::endl;

        return 0;
     }
     * @endcode
     */
    class Basinhopping {
    public:
        /**
         * Configurable options for modifying the default behaviour of the basinhopping algorithm.
         */
        struct Options {
            /**
             * @brief Default constructor
             * @details This tries to set up reasonable defaults for optimization. It is highly
             * recommended that the user read and overwrite the defaults based on the cost function.
             */
            Options() {
                local_minimizer_options = GradientLocalMinimizer::Options();
                scoped_ptr<StepFunction> default_step(new DefaultStepFunction(1.0));
                step_function.swap(default_step);
                max_iterations = 100;
                max_stagnant_iterations = 20;
                minimum_cost = DBL_MIN;
                is_silent = true;
            }

            /**
             * @brief Convenience function for changing the default step function.
             * @details This function simply swaps the scoped_ptr to the user 
             * defined `StepFunction` with the scoped_ptr to the `StepFunction`
             * held within the `pallas::Basinhopping::Options` struct.
             * 
             * @param user_step_function pallas::scoped_ptr<pallas::StepFunction>. This function generates radomized candidate solutions based on the current position. 
             */
            void set_step_function(scoped_ptr<StepFunction>& user_step_function) {
                swap(user_step_function, step_function);
            }

            /**
             * Contains any changes to the default options for the local minimization algorthm. See the documentation for ceres::GradientProblemSolver::Options for relevant options
             */
            GradientLocalMinimizer::Options local_minimizer_options;

            /**
             * Function that produces randomized candidate solutions.
             */
            scoped_ptr<StepFunction> step_function;

            /**
             * Maximum number of basinhopping iterations.
             */
            unsigned int max_iterations;

            /**
             * Maximum number sequential basinhopping iterations allowed without finding a new global minimum.
             */
            unsigned int max_stagnant_iterations;

            /**
             * User specified minimum cost. Minimization will halt if `minimum_cost` is reached.
             */
            double minimum_cost;

            /**
             * Whether to log failure information relating the to global optimization algorithm using glog.
             */
            bool is_silent;

        };
            /**
             * @brief Contains a summary of the optimization.
             * @details This struct contains the result of the optimization and has convenience methods for printing reports of a completed optimization.
             */
        struct Summary {
            /**
             * @brief Default constuctor
             */
            Summary();
            std::string BriefReport() const;/**<A brief one line description of the state of the solver after termination.*/
 
            std::string FullReport() const;/**<A full multi-line description of the state of the solver after termination.*/

            // Minimizer summary -------------------------------------------------
            LineSearchDirectionType line_search_direction_type;/**<Method to find the next search direction. Valid options are `STEEPEST_DESCENT`, `NONLINEAR_CONJUGATE_GRADIENT`, `LBFGS`, or `BFGS`*/

            TerminationType termination_type;/**<Reason optimization was terminated*/
 
            std::string message;/**<Message describing why the solver terminated.*/

            double initial_cost;/**<Cost of the problem (value of the objective function) before the optimization.*/
 
            double final_cost;/**<Cost of the problem (value of the objective function) after the optimization.*/

            GradientLocalMinimizer::Summary local_minimization_summary;/**<Summary from the last local minimization iteration.*/

            unsigned int num_parameters;/**<Number of parameters in the problem.*/

            unsigned int num_iterations;/**<Number of basinhopping iterations*/

            double total_time_in_seconds;/**<total time elapsed in global minimizer*/

            double local_minimization_time_in_seconds;/**<time spent in local minimizer*/

            double step_time_in_seconds;/**<time spent calling step function*/

            double cost_evaluation_time_in_seconds;/**<time spent evaluating cost function (outside local minimization)*/
        };

        /**
         * @brief Default constructor
         */
        Basinhopping() {};

        /**
         * @brief Minimizes the specified gradient problem.
         * @details The specified options are used to setup a basinhopping instance which
         * is then used to minimize the GradientProblem. The optimal solution is stored
         * in `parameters` and a summary of the global optimization can be found in `summary`.
         * 
         * @param options pallas::Basinhopping::Options. Options used to configure the optimization.
         * @param problem pallas::GradientProblem. The problem to optimize.
         * @param parameters double*. The starting point for further optimization.
         * @param summary Basinhopping::Summary*. Summary instance to store the optimization details.
         */
        void Solve(const Basinhopping::Options& options,
                   const GradientProblem& problem,
                   double* parameters,
                   Basinhopping::Summary*global_summary);

    private:
        /**
         * @brief Checks to see if any temination conditions were met.
         * 
         * @param options pallas::Basinhopping::Options. Options used to configure the optimization.
         * @param message std::string*. If a termination condition is met, a message describing the satified condition is stored in the variable. 
         * @param termination_type pallas::TerminationType*. This
         * @return Returns `true` if a termination condition was meet, `false` otherwise. 
         */
        bool check_for_termination_(const Basinhopping::Options &options,
                                    std::string* message,
                                    TerminationType* termination_type);
        /**
         * @brief Updates the global summary before exiting the basinhopping algorithm.
         */
        void prepare_final_summary_(Basinhopping::Summary* global_summary,
                                   const GradientLocalMinimizer::Summary& local_summary);

        internal::Metropolis metropolis;/**<Determines whether to accept a higher cost candidate solution*/

        internal::State current_state;/**<The current state of the optimization*/
        internal::State candidate_state;/**<A randomized candidate solution that is then minimized using a local minimization algorithm and compared to the current state.*/
        internal::State global_minimum_state;/**The best solution found during any stage of the optimization. It is this value that is returned when minimization concludes.*/

        unsigned int num_stagnant_iterations;/**<The number of iterations which has elapsed without finding a new global minimum.*/
        unsigned int num_iterations;/**<The number of local optimization iterations the global optimizer has performed.*/

    };

    /**
     * @brief Helper function which avoids going through the interface of the pallas::Basinhopping class.
     * @details The specified options are used to setup a basinhopping instance which
     * is then used to minimize the GradientProblem. The optimal solution is stored
     * in `parameters` and a summary of the global optimization can be found in `summary`.
     * 
     * @param options pallas::Basinhopping::Options. Options used to configure the optimization.
     * @param problem pallas::GradientProblem. The problem to optimize.
     * @param parameters double*. The starting point for further optimization.
     * @param summary Basinhopping::Summary*. Summary instance to store the optimization details.
     */
    void Solve(const Basinhopping::Options& options,
               const GradientProblem& problem,
               double* parameters,
               Basinhopping::Summary* summary);

} // namespace pallas

#endif //PALLAS_BASINHOPPING_H