/*********************************************************************
*  Copyright (c) 2017 Robert Bosch GmbH.
*  All rights reserved.
*
*  Licensed under the Apache License, Version 2.0 (the "License");
*  you may not use this file except in compliance with the License.
*  You may obtain a copy of the License at
*
*      http://www.apache.org/licenses/LICENSE-2.0
*
*  Unless required by applicable law or agreed to in writing, software
*  distributed under the License is distributed on an "AS IS" BASIS,
*  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
*  See the License for the specific language governing permissions and
*  limitations under the License.
* *********************************************************************/

#include <gtest/gtest.h>
#include <time.h>
#include <fstream>
#include <iostream>

#include <ros/package.h>

#include "steering_functions/hc_cc_state_space/cc00_reeds_shepp_state_space.hpp"
#include "steering_functions/hc_cc_state_space/hc00_reeds_shepp_state_space.hpp"
#include "steering_functions/hc_cc_state_space/hc0pm_reeds_shepp_state_space.hpp"
#include "steering_functions/hc_cc_state_space/hc_reeds_shepp_state_space.hpp"
#include "steering_functions/hc_cc_state_space/hcpm0_reeds_shepp_state_space.hpp"
#include "steering_functions/hc_cc_state_space/hcpmpm_reeds_shepp_state_space.hpp"
#include "steering_functions/reeds_shepp_state_space/reeds_shepp_state_space.hpp"
#include "steering_functions/steering_functions.hpp"

using namespace std;
using namespace steer;

#define EPS_KAPPA 1e-3                   // [1/m]
#define KAPPA 1.0                        // [1/m]
#define SIGMA 1.0                        // [1/m^2]
#define DISCRETIZATION 0.1               // [m]
#define SAMPLES 1e5                      // [-]
#define OPERATING_REGION_X 20.0          // [m]
#define OPERATING_REGION_Y 20.0          // [m]
#define OPERATING_REGION_THETA 2 * M_PI  // [rad]
#define ALPHA1 0.1                       // [-]
#define ALPHA2 0.05                      // [-]
#define ALPHA3 0.05                      // [-]
#define ALPHA4 0.1                       // [-]
#define STD_X 0.1                        // [m]
#define STD_Y 0.1                        // [m]
#define STD_THETA 0.05                   // [rad]
#define K1 1.5                           // [-]
#define K2 0.25                          // [-]
#define K3 1.0                           // [-]
#define random(lower, upper) (rand() * (upper - lower) / RAND_MAX + lower)

struct Statistic
{
  State start;
  State goal;
  double computation_time;
  double path_length;
  int curv_discont;
};

State get_random_state()
{
  State state;
  state.x = random(-OPERATING_REGION_X / 2.0, OPERATING_REGION_X / 2.0);
  state.y = random(-OPERATING_REGION_Y / 2.0, OPERATING_REGION_Y / 2.0);
  state.theta = random(-OPERATING_REGION_THETA / 2.0, OPERATING_REGION_THETA / 2.0);
  state.kappa = random(-KAPPA, KAPPA);
  state.d = 0.0;
  return state;
}

double get_mean(const vector<double>& v)
{
  double sum = accumulate(v.begin(), v.end(), 0.0);
  return sum / v.size();
}

double get_std(const vector<double>& v)
{
  double mean = get_mean(v);
  double diff_sq = 0;
  for (const auto& x : v)
  {
    diff_sq += pow(x - mean, 2);
  }
  return sqrt(diff_sq / v.size());
}

int get_curv_discont(const vector<Control>& controls)
{
  int curv_discont = 0;
  vector<Control>::const_iterator iter1 = controls.begin();
  for (vector<Control>::const_iterator iter2 = next(controls.begin()); iter2 != controls.end(); ++iter2)
  {
    if (fabs(iter1->kappa + iter1->sigma * fabs(iter1->delta_s) - iter2->kappa) > EPS_KAPPA)
      ++curv_discont;
    iter1 = iter2;
  }
  return curv_discont;
}

void write_to_file(const string& id, const vector<Statistic>& stats)
{
  string path_to_output = ros::package::getPath("steering_functions") + "/test/" + id + "_stats.csv";
  remove(path_to_output.c_str());
  string header = "start,goal,computation_time,path_length";
  fstream f;
  f.open(path_to_output, ios::app);
  f << header << endl;
  for (const auto& stat : stats)
  {
    State start = stat.start;
    State goal = stat.goal;
    f << start.x << " " << start.y << " " << start.theta << " " << start.kappa << " " << start.d << ",";
    f << goal.x << " " << goal.y << " " << goal.theta << " " << goal.kappa << " " << goal.d << ",";
    f << stat.computation_time << "," << stat.path_length << "," << stat.curv_discont << endl;
  }
}

CC00_Reeds_Shepp_State_Space cc00_rs_ss(KAPPA, SIGMA, DISCRETIZATION);
HC_Reeds_Shepp_State_Space hc_rs_ss(KAPPA, SIGMA, DISCRETIZATION);
HC00_Reeds_Shepp_State_Space hc00_rs_ss(KAPPA, SIGMA, DISCRETIZATION);
HC0pm_Reeds_Shepp_State_Space hc0pm_rs_ss(KAPPA, SIGMA, DISCRETIZATION);
HCpm0_Reeds_Shepp_State_Space hcpm0_rs_ss(KAPPA, SIGMA, DISCRETIZATION);
HCpmpm_Reeds_Shepp_State_Space hcpmpm_rs_ss(KAPPA, SIGMA, DISCRETIZATION);
Reeds_Shepp_State_Space rs_ss(KAPPA, DISCRETIZATION);

vector<Statistic> get_controls(const string& id, const vector<State>& starts, const vector<State>& goals)
{
  clock_t clock_start;
  clock_t clock_finish;
  Statistic stat;
  vector<Statistic> stats;
  stats.reserve(SAMPLES);
  double path_length;
  int curv_discont;
  for (auto start = starts.begin(), goal = goals.begin(); start != starts.end(); ++start, ++goal)
  {
    if (id == "HCpmpm_RS")
    {
      clock_start = clock();
      vector<Control> hcpmpm_rs_controls = hcpmpm_rs_ss.get_controls(*start, *goal);
      clock_finish = clock();
      path_length = hcpmpm_rs_ss.get_distance(*start, *goal);
      curv_discont = get_curv_discont(hcpmpm_rs_controls);
    }
    else if (id == "RS")
    {
      clock_start = clock();
      vector<Control> rs_controls = rs_ss.get_controls(*start, *goal);
      clock_finish = clock();
      path_length = rs_ss.get_distance(*start, *goal);
      curv_discont = get_curv_discont(rs_controls);
    }
    stat.start = *start;
    stat.goal = *goal;
    stat.computation_time = double(clock_finish - clock_start) / CLOCKS_PER_SEC;
    stat.path_length = path_length;
    stat.curv_discont = curv_discont;
    stats.push_back(stat);
  }
  return stats;
}

vector<Statistic> get_path(const string& id, const vector<State>& starts, const vector<State>& goals)
{
  clock_t clock_start;
  clock_t clock_finish;
  Statistic stat;
  vector<Statistic> stats;
  stats.reserve(SAMPLES);
  for (auto start = starts.begin(), goal = goals.begin(); start != starts.end(); ++start, ++goal)
  {
    if (id == "HCpmpm_RS")
    {
      clock_start = clock();
      hcpmpm_rs_ss.get_path(*start, *goal);
      clock_finish = clock();
    }
    else if (id == "RS")
    {
      clock_start = clock();
      rs_ss.get_path(*start, *goal);
      clock_finish = clock();
    }
    stat.start = *start;
    stat.goal = *goal;
    stat.computation_time = double(clock_finish - clock_start) / CLOCKS_PER_SEC;
    stats.push_back(stat);
  }
  return stats;
}

vector<Statistic> get_path_with_covariance(const string& id, const vector<State_With_Covariance>& starts,
                                           const vector<State>& goals)
{
  clock_t clock_start;
  clock_t clock_finish;
  Statistic stat;
  vector<Statistic> stats;
  stats.reserve(SAMPLES);
  auto start = starts.begin();
  auto goal = goals.begin();
  for (start = starts.begin(), goal = goals.begin(); start != starts.end(); ++start, ++goal)
  {
    if (id == "HCpmpm_RS")
    {
      clock_start = clock();
      hcpmpm_rs_ss.get_path_with_covariance(*start, *goal);
      clock_finish = clock();
    }
    else if (id == "RS")
    {
      clock_start = clock();
      rs_ss.get_path_with_covariance(*start, *goal);
      clock_finish = clock();
    }
    stat.start = start->state;
    stat.goal = *goal;
    stat.computation_time = double(clock_finish - clock_start) / CLOCKS_PER_SEC;
    stats.push_back(stat);
  }
  return stats;
}

TEST(Timing, getControls)
{
  srand(0);
  vector<double> computation_times;
  computation_times.reserve(SAMPLES);
  vector<State> starts_with_curvature, starts_without_curvature;
  starts_with_curvature.reserve(SAMPLES);
  starts_without_curvature.reserve(SAMPLES);
  vector<State> goals_with_curvature, goals_without_curvature;
  goals_with_curvature.reserve(SAMPLES);
  goals_without_curvature.reserve(SAMPLES);
  for (int i = 0; i < SAMPLES; i++)
  {
    State start = get_random_state();
    State goal = get_random_state();
    starts_with_curvature.push_back(start);
    goals_with_curvature.push_back(goal);
    start.kappa = 0.0;
    goal.kappa = 0.0;
    starts_without_curvature.push_back(start);
    goals_without_curvature.push_back(goal);
  }

  string cc_dubins_id = "CC_Dubins";
  vector<Statistic> cc_dubins_stats = get_controls(cc_dubins_id, starts_with_curvature, goals_with_curvature);
  computation_times.clear();
  for (const auto& stat : cc_dubins_stats)
  {
    computation_times.push_back(stat.computation_time);
  }
  cout << "[----------] " + cc_dubins_id + " mean [s] +/- std [s]: " << get_mean(computation_times) << " +/- "
       << get_std(computation_times) << endl;

  string cc00_dubins_id = "CC00_Dubins";
  vector<Statistic> cc00_dubins_stats = get_controls(cc00_dubins_id, starts_without_curvature, goals_without_curvature);
  computation_times.clear();
  for (const auto& stat : cc00_dubins_stats)
  {
    computation_times.push_back(stat.computation_time);
  }
  cout << "[----------] " + cc00_dubins_id + " mean [s] +/- std [s]: " << get_mean(computation_times) << " +/- "
       << get_std(computation_times) << endl;

  string hcpmpm_rs_id = "HCpmpm_RS";
  vector<Statistic> hcpmpm_rs_stats = get_controls(hcpmpm_rs_id, starts_without_curvature, goals_without_curvature);
  computation_times.clear();
  for (const auto& stat : hcpmpm_rs_stats)
  {
    computation_times.push_back(stat.computation_time);
  }
  cout << "[----------] " + hcpmpm_rs_id + " mean [s] +/- std [s]: " << get_mean(computation_times) << " +/- "
       << get_std(computation_times) << endl;

  string rs_id = "RS";
  vector<Statistic> rs_stats = get_controls(rs_id, starts_without_curvature, goals_without_curvature);
  computation_times.clear();
  for (const auto& stat : rs_stats)
  {
    computation_times.push_back(stat.computation_time);
  }
  cout << "[----------] " + rs_id + " mean [s] +/- std [s]: " << get_mean(computation_times) << " +/- "
       << get_std(computation_times) << endl;

  //  write_to_file(cc_dubins_id, cc_dubins_stats);
  //  write_to_file(cc00_dubins_id, cc00_dubins_stats);
  //  write_to_file(cc0pm_dubins_id, cc0pm_dubins_stats);
  //  write_to_file(ccpm0_dubins_id, ccpm0_dubins_stats);
  //  write_to_file(ccpmpm_dubins_id, ccpmpm_dubins_stats);
  //  write_to_file(dubins_id, dubins_stats);
  //  write_to_file(cc00_rs_id, cc00_rs_stats);
  //  write_to_file(hc_rs_id, hc_rs_stats);
  //  write_to_file(hc00_rs_id, hc00_rs_stats);
  //  write_to_file(hc0pm_rs_id, hc0pm_rs_stats);
  //  write_to_file(hcpm0_rs_id, hcpm0_rs_stats);
  //  write_to_file(hcpmpm_rs_id, hcpmpm_rs_stats);
  //  write_to_file(rs_id, rs_stats);
}

TEST(Timing, getPath)
{
  srand(0);
  vector<double> computation_times;
  computation_times.reserve(SAMPLES);
  vector<State> starts_with_curvature, starts_without_curvature;
  starts_with_curvature.reserve(SAMPLES);
  starts_without_curvature.reserve(SAMPLES);
  vector<State> goals_with_curvature, goals_without_curvature;
  goals_with_curvature.reserve(SAMPLES);
  goals_without_curvature.reserve(SAMPLES);
  for (int i = 0; i < SAMPLES; i++)
  {
    State start = get_random_state();
    State goal = get_random_state();
    starts_with_curvature.push_back(start);
    goals_with_curvature.push_back(goal);
    start.kappa = 0.0;
    goal.kappa = 0.0;
    starts_without_curvature.push_back(start);
    goals_without_curvature.push_back(goal);
  }

  string cc_dubins_id = "CC_Dubins";
  vector<Statistic> cc_dubins_stats = get_path(cc_dubins_id, starts_with_curvature, goals_with_curvature);
  computation_times.clear();
  for (const auto& stat : cc_dubins_stats)
  {
    computation_times.push_back(stat.computation_time);
  }
  cout << "[----------] " + cc_dubins_id + " mean [s] +/- std [s]: " << get_mean(computation_times) << " +/- "
       << get_std(computation_times) << endl;

  string cc00_dubins_id = "CC00_Dubins";
  vector<Statistic> cc00_dubins_stats = get_path(cc00_dubins_id, starts_without_curvature, goals_without_curvature);
  computation_times.clear();
  for (const auto& stat : cc00_dubins_stats)
  {
    computation_times.push_back(stat.computation_time);
  }
  cout << "[----------] " + cc00_dubins_id + " mean [s] +/- std [s]: " << get_mean(computation_times) << " +/- "
       << get_std(computation_times) << endl;

  string hcpmpm_rs_id = "HCpmpm_RS";
  vector<Statistic> hcpmpm_rs_stats = get_path(hcpmpm_rs_id, starts_without_curvature, goals_without_curvature);
  computation_times.clear();
  for (const auto& stat : hcpmpm_rs_stats)
  {
    computation_times.push_back(stat.computation_time);
  }
  cout << "[----------] " + hcpmpm_rs_id + " mean [s] +/- std [s]: " << get_mean(computation_times) << " +/- "
       << get_std(computation_times) << endl;

  string rs_id = "RS";
  vector<Statistic> rs_stats = get_path(rs_id, starts_without_curvature, goals_without_curvature);
  computation_times.clear();
  for (const auto& stat : rs_stats)
  {
    computation_times.push_back(stat.computation_time);
  }
  cout << "[----------] " + rs_id + " mean [s] +/- std [s]: " << get_mean(computation_times) << " +/- "
       << get_std(computation_times) << endl;
}

TEST(Timing, getPathWithCovariance)
{
  // set filter parameters
  Motion_Noise motion_noise;
  Measurement_Noise measurement_noise;
  Controller controller;
  motion_noise.alpha1 = ALPHA1;
  motion_noise.alpha2 = ALPHA2;
  motion_noise.alpha3 = ALPHA3;
  motion_noise.alpha4 = ALPHA4;
  measurement_noise.std_x = STD_X;
  measurement_noise.std_y = STD_Y;
  measurement_noise.std_theta = STD_THETA;
  controller.k1 = K1;
  controller.k2 = K2;
  controller.k3 = K3;
  cc_dubins_forwards_ss.set_filter_parameters(motion_noise, measurement_noise, controller);
  cc00_dubins_forwards_ss.set_filter_parameters(motion_noise, measurement_noise, controller);
  cc0pm_dubins_forwards_ss.set_filter_parameters(motion_noise, measurement_noise, controller);
  ccpm0_dubins_forwards_ss.set_filter_parameters(motion_noise, measurement_noise, controller);
  ccpmpm_dubins_forwards_ss.set_filter_parameters(motion_noise, measurement_noise, controller);
  dubins_forwards_ss.set_filter_parameters(motion_noise, measurement_noise, controller);
  dubins_backwards_ss.set_filter_parameters(motion_noise, measurement_noise, controller);
  cc00_rs_ss.set_filter_parameters(motion_noise, measurement_noise, controller);
  hc_rs_ss.set_filter_parameters(motion_noise, measurement_noise, controller);
  hc00_rs_ss.set_filter_parameters(motion_noise, measurement_noise, controller);
  hc0pm_rs_ss.set_filter_parameters(motion_noise, measurement_noise, controller);
  hcpm0_rs_ss.set_filter_parameters(motion_noise, measurement_noise, controller);
  hcpmpm_rs_ss.set_filter_parameters(motion_noise, measurement_noise, controller);
  rs_ss.set_filter_parameters(motion_noise, measurement_noise, controller);

  srand(0);
  vector<double> computation_times;
  computation_times.reserve(SAMPLES);
  vector<State_With_Covariance> starts_with_curvature, starts_without_curvature;
  starts_with_curvature.reserve(SAMPLES);
  starts_without_curvature.reserve(SAMPLES);
  vector<State> goals_with_curvature, goals_without_curvature;
  goals_with_curvature.reserve(SAMPLES);
  goals_without_curvature.reserve(SAMPLES);
  for (int i = 0; i < SAMPLES; i++)
  {
    State_With_Covariance start;
    start.state = get_random_state();
    start.covariance[0 + 4 * 0] = start.Sigma[0 + 4 * 0] = pow(STD_X, 2);
    start.covariance[1 + 4 * 1] = start.Sigma[1 + 4 * 1] = pow(STD_Y, 2);
    start.covariance[2 + 4 * 2] = start.Sigma[2 + 4 * 2] = pow(STD_THETA, 2);
    State goal = get_random_state();
    starts_with_curvature.push_back(start);
    goals_with_curvature.push_back(goal);
    start.state.kappa = 0.0;
    goal.kappa = 0.0;
    starts_without_curvature.push_back(start);
    goals_without_curvature.push_back(goal);
  }

  string hcpmpm_rs_id = "HCpmpm_RS";
  vector<Statistic> hcpmpm_rs_stats =
      get_path_with_covariance(hcpmpm_rs_id, starts_without_curvature, goals_without_curvature);
  computation_times.clear();
  for (const auto& stat : hcpmpm_rs_stats)
  {
    computation_times.push_back(stat.computation_time);
  }
  cout << "[----------] " + hcpmpm_rs_id + " mean [s] +/- std [s]: " << get_mean(computation_times) << " +/- "
       << get_std(computation_times) << endl;

  string rs_id = "RS";
  vector<Statistic> rs_stats = get_path_with_covariance(rs_id, starts_without_curvature, goals_without_curvature);
  computation_times.clear();
  for (const auto& stat : rs_stats)
  {
    computation_times.push_back(stat.computation_time);
  }
  cout << "[----------] " + rs_id + " mean [s] +/- std [s]: " << get_mean(computation_times) << " +/- "
       << get_std(computation_times) << endl;
}

int main(int argc, char** argv)
{
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
