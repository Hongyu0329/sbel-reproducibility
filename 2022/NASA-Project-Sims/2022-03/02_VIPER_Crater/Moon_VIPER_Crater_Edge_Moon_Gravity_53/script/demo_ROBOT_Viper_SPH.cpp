// =============================================================================
// PROJECT CHRONO - http://projectchrono.org
//
// Copyright (c) 2014 projectchrono.org
// All rights reserved.
//
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file at the top level of the distribution and at
// http://projectchrono.org/license-chrono.txt.
//
// =============================================================================
// Author: Wei Hu, Jason Zhou
// Chrono::FSI demo to show usage of VIPER rover models on SPH granular terrain
// This demo uses a plug-in VIPER rover model from chrono::models
// =============================================================================

/// General Includes
#include <cassert>
#include <cstdlib>
#include <ctime>

/// Chrono includes
#include "chrono_models/robot/viper/Viper.h"

#include "chrono/physics/ChSystemNSC.h"
#include "chrono/utils/ChUtilsCreators.h"
#include "chrono/utils/ChUtilsGenerators.h"
#include "chrono/utils/ChUtilsGeometry.h"
#include "chrono/utils/ChUtilsInputOutput.h"
#include "chrono/assets/ChBoxShape.h"
#include "chrono/physics/ChBodyEasy.h"
#include "chrono/physics/ChInertiaUtils.h"
#include "chrono/physics/ChParticlesClones.h"
#include "chrono/assets/ChTriangleMeshShape.h"
#include "chrono/geometry/ChTriangleMeshConnected.h"
#include "chrono/physics/ChLinkMotorRotationSpeed.h"
#include "chrono/physics/ChLinkMotorRotationTorque.h"
#include "chrono/physics/ChLinkDistance.h"

/// Chrono fsi includes
#include "chrono_fsi/ChSystemFsi.h"

/// Chrono namespaces
using namespace chrono;
using namespace chrono::fsi;
using namespace chrono::geometry;
using namespace chrono::viper;

/// output directories and settings
const std::string out_dir = GetChronoOutputPath() + "FSI_VIPER/";
std::string demo_dir;
bool pv_output = true;
bool save_obj = false;  // if true, save as Wavefront OBJ
bool save_vtk = false;  // if true, save as VTK
double rock_scale = 0.3;

double smalldis = 1.0e-9;

/// Dimension of the space domain
double bxDim = 1.0 + smalldis;
double byDim = 1.0 + smalldis;
double bzDim = 0.2 + smalldis;

/// Dimension of the terrain domain
double fxDim = 1.0 + smalldis;
double fyDim = 1.0 + smalldis;
double fzDim = 0.1 + smalldis;

/// Pointer to store the VIPER instance
std::shared_ptr<Viper> rover_one;
// std::shared_ptr<Viper> rover_two;
// std::shared_ptr<Viper> rover_three;

/// Pointer to store the VIPER driver
std::shared_ptr<ViperSpeedDriver> driver1;
// std::shared_ptr<ViperSpeedDriver> driver2;
// std::shared_ptr<ViperSpeedDriver> driver3;

void CreateMeshMarkers(std::shared_ptr<geometry::ChTriangleMeshConnected> mesh,
                       double delta,
                       std::vector<ChVector<>>& point_cloud) {
    mesh->RepairDuplicateVertexes(1e-9);  // if meshes are not watertight

    ChVector<> minV = mesh->m_vertices[0];
    ChVector<> maxV = mesh->m_vertices[0];
    ChVector<> currV = mesh->m_vertices[0];
    for (unsigned int i = 1; i < mesh->m_vertices.size(); ++i) {
        currV = mesh->m_vertices[i];
        if (minV.x() > currV.x())
            minV.x() = currV.x();
        if (minV.y() > currV.y())
            minV.y() = currV.y();
        if (minV.z() > currV.z())
            minV.z() = currV.z();
        if (maxV.x() < currV.x())
            maxV.x() = currV.x();
        if (maxV.y() < currV.y())
            maxV.y() = currV.y();
        if (maxV.z() < currV.z())
            maxV.z() = currV.z();
    }
    ////printf("start coords: %f, %f, %f\n", minV.x(), minV.y(), minV.z());
    ////printf("end coords: %f, %f, %f\n", maxV.x(), maxV.y(), maxV.z());

    const double EPSI = 1e-6;

    ChVector<> ray_origin;
    for (double x = minV.x(); x < maxV.x(); x += delta) {
        ray_origin.x() = x + 1e-9;
        for (double y = minV.y(); y < maxV.y(); y += delta) {
            ray_origin.y() = y + 1e-9;
            for (double z = minV.z(); z < maxV.z(); z += delta) {
                ray_origin.z() = z + 1e-9;

                ChVector<> ray_dir[2] = {ChVector<>(5, 0.5, 0.25), ChVector<>(-3, 0.7, 10)};
                int intersectCounter[2] = {0, 0};

                for (unsigned int i = 0; i < mesh->m_face_v_indices.size(); ++i) {
                    auto& t_face = mesh->m_face_v_indices[i];
                    auto& v1 = mesh->m_vertices[t_face.x()];
                    auto& v2 = mesh->m_vertices[t_face.y()];
                    auto& v3 = mesh->m_vertices[t_face.z()];

                    // Find vectors for two edges sharing V1
                    auto edge1 = v2 - v1;
                    auto edge2 = v3 - v1;

                    bool t_inter[2] = {false, false};

                    for (unsigned int j = 0; j < 2; j++) {
                        // Begin calculating determinant - also used to calculate uu parameter
                        auto pvec = Vcross(ray_dir[j], edge2);
                        // if determinant is near zero, ray is parallel to plane of triangle
                        double det = Vdot(edge1, pvec);
                        // NOT CULLING
                        if (det > -EPSI && det < EPSI) {
                            t_inter[j] = false;
                            continue;
                        }
                        double inv_det = 1.0 / det;

                        // calculate distance from V1 to ray origin
                        auto tvec = ray_origin - v1;

                        // Calculate uu parameter and test bound
                        double uu = Vdot(tvec, pvec) * inv_det;
                        // The intersection lies outside of the triangle
                        if (uu < 0.0 || uu > 1.0) {
                            t_inter[j] = false;
                            continue;
                        }

                        // Prepare to test vv parameter
                        auto qvec = Vcross(tvec, edge1);

                        // Calculate vv parameter and test bound
                        double vv = Vdot(ray_dir[j], qvec) * inv_det;
                        // The intersection lies outside of the triangle
                        if (vv < 0.0 || ((uu + vv) > 1.0)) {
                            t_inter[j] = false;
                            continue;
                        }

                        double tt = Vdot(edge2, qvec) * inv_det;
                        if (tt > EPSI) {  // ray intersection
                            t_inter[j] = true;
                            continue;
                        }

                        // No hit, no win
                        t_inter[j] = false;
                    }

                    intersectCounter[0] += t_inter[0] ? 1 : 0;
                    intersectCounter[1] += t_inter[1] ? 1 : 0;
                }

                if (((intersectCounter[0] % 2) == 1) && ((intersectCounter[1] % 2) == 1))  // inside mesh
                    point_cloud.push_back(ChVector<>(x, y, z));
            }
        }
    }
}

std::shared_ptr<ChMaterialSurface> CustomWheelMaterial(ChContactMethod contact_method) {
    float mu = 0.9f;   // coefficient of friction
    float cr = 0.4f;   // coefficient of restitution
    float Y = 2e7f;    // Young's modulus
    float nu = 0.3f;   // Poisson ratio
    float kn = 2e5f;   // normal stiffness
    float gn = 40.0f;  // normal viscous damping
    float kt = 2e5f;   // tangential stiffness
    float gt = 20.0f;  // tangential viscous damping

    switch (contact_method) {
        case ChContactMethod::NSC: {
            auto matNSC = chrono_types::make_shared<ChMaterialSurfaceNSC>();
            matNSC->SetFriction(mu);
            matNSC->SetRestitution(cr);
            return matNSC;
        }
        case ChContactMethod::SMC: {
            auto matSMC = chrono_types::make_shared<ChMaterialSurfaceSMC>();
            matSMC->SetFriction(mu);
            matSMC->SetRestitution(cr);
            matSMC->SetYoungModulus(Y);
            matSMC->SetPoissonRatio(nu);
            matSMC->SetKn(kn);
            matSMC->SetGn(gn);
            matSMC->SetKt(kt);
            matSMC->SetGt(gt);
            return matSMC;
        }
        default:
            return std::shared_ptr<ChMaterialSurface>();
    }
}

/// Forward declaration of helper functions
void SaveParaViewFiles(ChSystemFsi& myFsiSystem,
                       ChSystemNSC& mphysicalSystem,
                       std::shared_ptr<fsi::SimParams> paramsH,
                       int tStep,
                       double mTime);

void AddWall(std::shared_ptr<ChBody> body,
             const ChVector<>& dim,
             std::shared_ptr<ChMaterialSurface> mat,
             const ChVector<>& loc) {
    body->GetCollisionModel()->AddBox(mat, dim.x(), dim.y(), dim.z(), loc);
    auto box = chrono_types::make_shared<ChBoxShape>();
    box->GetBoxGeometry().Size = dim;
    box->GetBoxGeometry().Pos = loc;
}

void CreateSolidPhase(ChSystemNSC& mphysicalSystem, 
                      ChSystemFsi& myFsiSystem, 
                      std::shared_ptr<fsi::SimParams> paramsH);

void ShowUsage() {
    std::cout << "usage: ./demo_FSI_Granular_Viper <json_file>" << std::endl;
}

int main(int argc, char* argv[]) {
    /// Set path to Chrono data directories
    SetChronoDataPath(CHRONO_DATA_DIR);

    /// Create a physical system and a corresponding FSI system
    ChSystemNSC mphysicalSystem;
    ChSystemFsi myFsiSystem(mphysicalSystem);

    /// Get the pointer to the system parameter and use a JSON file to fill it out with the user parameters
    std::shared_ptr<fsi::SimParams> paramsH = myFsiSystem.GetSimParams();
    std::string inputJson = GetChronoDataFile("fsi/input_json/demo_FSI_Viper_granular_NSC.json");
    if (argc == 1) {
        std::cout << "Use the default JSON file" << std::endl;
    } else if (argc == 2) {
        std::cout << "Use the specified JSON file" << std::endl;
        std::string my_inputJson = std::string(argv[1]);
        inputJson = my_inputJson;
    } else {
        ShowUsage();
        return 1;
    }
    myFsiSystem.SetSimParameter(inputJson, paramsH, ChVector<>(bxDim, byDim, bzDim));

    /// Set SPH discretization type, consistent or inconsistent
    myFsiSystem.SetDiscreType(false, false);

    /// Set wall boundary condition
    myFsiSystem.SetWallBC(BceVersion::ORIGINAL);

    /// Reset the domain size
    bxDim = paramsH->boxDimX + smalldis;
    byDim = paramsH->boxDimY + smalldis;
    bzDim = paramsH->boxDimZ + smalldis;

    fxDim = paramsH->fluidDimX + smalldis;
    fyDim = paramsH->fluidDimY + smalldis;
    fzDim = paramsH->fluidDimZ + smalldis;

    /// Setup the solver based on the input value of the prameters
    myFsiSystem.SetFluidDynamics(paramsH->fluid_dynamic_type);

    /// Set the periodic boundary condition
    double initSpace0 = paramsH->MULT_INITSPACE * paramsH->HSML;
    ChVector<> cMin(-bxDim / 2 * 2, -byDim / 2 * 2, -bzDim * 20);
    ChVector<> cMax(bxDim / 2 * 2, byDim / 2 * 2, bzDim * 20);
    myFsiSystem.SetBoundaries(cMin, cMax, paramsH);

    /// Setup sub doamins for a faster neighbor particle searching
    myFsiSystem.SetSubDomain(paramsH);

    /// Setup the output directory for FSI data
    myFsiSystem.SetFsiOutputDir(paramsH, demo_dir, out_dir, inputJson.c_str());

    /// Set FSI information output
    myFsiSystem.SetFsiInfoOutput(false);

    /// Set simulation data output length
    myFsiSystem.SetOutputLength(0);

    /// Create an initial box for the terrain patch
    /*chrono::utils::GridSampler<> sampler(initSpace0);
    /// Use a chrono sampler to create a bucket of granular material
    ChVector<> boxCenter(0, 0, fzDim / 2);
    ChVector<> boxHalfDim(fxDim / 2, fyDim / 2, fzDim / 2);
    std::vector<ChVector<>> points = sampler.SampleBox(boxCenter, boxHalfDim);
    /// Add SPH particles from the sampler points to the FSI system
    int numPart = (int)points.size();
    for (int i = 0; i < numPart; i++) {
        double pre_ini = paramsH->rho0 * abs(paramsH->gravity.z) * (-points[i].z() + fzDim);
        myFsiSystem.AddSphMarker(points[i], paramsH->rho0, 0, paramsH->mu0, paramsH->HSML, -1,
                                 ChVector<>(0),         // initial velocity
                                 ChVector<>(-pre_ini),  // tauxxyyzz
                                 ChVector<>(0)          // tauxyxzyz
        );
    }*/
    /*int numPart = 0;
    for (int n = 2; n < 3; n++) {
        for (int m = 1; m < 2; m++) {
            chrono::utils::GridSampler<> sampler_heap(initSpace0);
            ChVector<> heapCenter(n * 1.0 - 2.0, m * 2.0 - 2.0, 0.4);
            ChVector<> heapHalfDim(0.2, 0.2, 0.2);
            std::vector<ChVector<>> points_heap = sampler_heap.SampleBox(heapCenter, heapHalfDim);
            int numPart_heap = (int)points_heap.size();
            for (int i = 0; i < numPart_heap; i++) {
                myFsiSystem.AddSphMarker(points_heap[i], paramsH->rho0, 0, paramsH->mu0, paramsH->HSML, -1,
                                        ChVector<>(0),  // initial velocity
                                        ChVector<>(0),  // tauxxyyzz
                                        ChVector<>(0)   // tauxyxzyz
                );
            }
            numPart = numPart + numPart_heap;
        }
    }*/
    int numPart = 0;
    {
        chrono::utils::GridSampler<> sampler_cyl(initSpace0);
        ChVector<> cylCenter(0.0, 0.0, 0.6);
        double cylRadiusOut = 2.0;
        double cylRadiusIn = 1.5;
        double cylHalfHeight = 0.5;
        std::vector<ChVector<>> points_cyl = sampler_cyl.SampleCylinderZ(cylCenter, cylRadiusOut, cylHalfHeight);
        int numPart_cyl = (int)points_cyl.size();
        int count_num = 0;
        for (int i = 0; i < numPart_cyl; i++) {
            double RR = points_cyl[i].x() * points_cyl[i].x() + points_cyl[i].y() * points_cyl[i].y();
            if (RR > cylRadiusIn * cylRadiusIn){
                myFsiSystem.AddSphMarker(points_cyl[i], paramsH->rho0, 0, paramsH->mu0, paramsH->HSML, -1,
                                         ChVector<>(0),  // initial velocity
                                         ChVector<>(0),  // tauxxyyzz
                                         ChVector<>(0)   // tauxyxzyz
                );
                count_num++;
            }
        }
        numPart = numPart + count_num;
    }
    myFsiSystem.AddRefArray(0, (int)numPart, -1, -1);

    /// Create MBD and BCE particles for the solid domain
    CreateSolidPhase(mphysicalSystem, myFsiSystem, paramsH);

    /// Construction of the FSI system must be finalized
    myFsiSystem.Finalize();

    /// Save data at the initial moment
    SaveParaViewFiles(myFsiSystem, mphysicalSystem, paramsH, 0, 0);

    /// Calculate the total steps
    double time = 0;
    double Global_max_dT = paramsH->dT_Max;
    int stepEnd = int(paramsH->tFinal / paramsH->dT);

    /// Add timing for ths simulation
    double TIMING_sta;
    double TIMING_end;
    double sim_cost = 0.0;

    // stepEnd = 10000;

    /// Print the body name and total number of bodies
    int num_body = mphysicalSystem.Get_bodylist().size();
    std::cout << "\n" << "Total number of bodies is " << num_body << "\n" << std::endl;
    for (int n = 0; n < num_body; n++) {
        auto bodynow = mphysicalSystem.Get_bodylist()[n];
        std::cout << "\n" << "Body " << n << " is: "<< bodynow->GetName() << std::endl;
    }

    // double lift_ang1 = 12.0 / 180.0 * CH_C_PI;
    // double lift_ang2 = 12.0 / 180.0 * CH_C_PI;
    // double lift_ang3 = 12.0 / 180.0 * CH_C_PI;
    // bool lift1 = false;
    // bool lift2 = false;
    // bool lift3 = false;
    for (int tStep = 0; tStep < stepEnd + 1; tStep++) {
        printf("\nstep : %d, time= : %f (s) \n", tStep, time);
        double frame_time = 1.0 / paramsH->out_fps;
        int next_frame = (int)floor((time + 1e-6) / frame_time) + 1;
        double next_frame_time = next_frame * frame_time;
        double max_allowable_dt = next_frame_time - time;
        if (max_allowable_dt > 1e-7)
            paramsH->dT_Max = std::min(Global_max_dT, max_allowable_dt);
        else
            paramsH->dT_Max = Global_max_dT;

        /// Update the rovers driver
        /*if (rover_one->GetChassis()->GetBody()->GetPos().x() > -2.50){
            driver1 = chrono_types::make_shared<ViperSpeedDriver>(0.1, -0.5 * CH_C_PI);
            lift1 = true;
        } else if (rover_one->GetChassis()->GetBody()->GetPos().x() < -4.2){
            driver1 = chrono_types::make_shared<ViperSpeedDriver>(0.1, 0.5 * CH_C_PI);
            if (lift1){
                lift_ang1 = lift_ang1 - 5.0 / 180.0 * CH_C_PI;
                lift1 = false;
            }
        }
        if (rover_two->GetChassis()->GetBody()->GetPos().y() > -2.75){
            driver2 = chrono_types::make_shared<ViperSpeedDriver>(0.1, -0.4 * CH_C_PI);
            lift2 = true;
        } else if (rover_two->GetChassis()->GetBody()->GetPos().y() < -4.0){
            driver2 = chrono_types::make_shared<ViperSpeedDriver>(0.1, 0.4 * CH_C_PI);
            if (lift2){
                lift_ang2 = lift_ang2 - 4.0 / 180.0 * CH_C_PI;
                lift2 = false;
            }
        }
        if (rover_three->GetChassis()->GetBody()->GetPos().y() < 2.35){
            driver3 = chrono_types::make_shared<ViperSpeedDriver>(0.1, -0.6 * CH_C_PI);
            lift3 = true;
        } else if (rover_three->GetChassis()->GetBody()->GetPos().y() > 4.0){
            driver3 = chrono_types::make_shared<ViperSpeedDriver>(0.1, 0.6 * CH_C_PI);
            if (lift3){
                lift_ang3 = lift_ang3 - 6.0 / 180.0 * CH_C_PI;
                lift3 = false;
            }
        }
        driver1->SetLifting(lift_ang1);
        driver2->SetLifting(lift_ang2);
        driver3->SetLifting(lift_ang3);
        rover_one->SetDriver(driver1);
        rover_two->SetDriver(driver2);
        rover_three->SetDriver(driver3);
        */

        /// Update the rovers
        rover_one->Update();
        // rover_two->Update();
        // rover_three->Update();

        TIMING_sta = clock();
        myFsiSystem.DoStepDynamics_FSI();
        TIMING_end = clock();
        sim_cost = sim_cost + (TIMING_end - TIMING_sta) / (double)CLOCKS_PER_SEC;

        time += paramsH->dT;
        SaveParaViewFiles(myFsiSystem, mphysicalSystem, paramsH, next_frame, time);

        auto rover_one_body = rover_one->GetChassis()->GetBody();
        // auto rover_two_body = rover_two->GetChassis()->GetBody();
        // auto rover_three_body = rover_three->GetChassis()->GetBody();
        printf("Rover_one=%f,%f,%f\n", rover_one_body->GetPos().x(), rover_one_body->GetPos().y(), rover_one_body->GetPos().z());
        // printf("Rover_two=%f,%f,%f\n", rover_two_body->GetPos().x(), rover_two_body->GetPos().y(), rover_two_body->GetPos().z());
        // printf("Rover_three=%f,%f,%f\n", rover_three_body->GetPos().x(), rover_three_body->GetPos().y(), rover_three_body->GetPos().z());
        printf("Physical time and computational cost = %f, %f\n", time, sim_cost);

        if (time > paramsH->tFinal)
            break;
    }

    return 0;
}

//------------------------------------------------------------------
// Create the objects of the MBD system. Rigid bodies, and if fsi,
// their BCE representation are created and added to the systems
//------------------------------------------------------------------
void CreateSolidPhase(ChSystemNSC& mphysicalSystem, 
                      ChSystemFsi& myFsiSystem, 
                      std::shared_ptr<fsi::SimParams> paramsH) {
    /// Set the gravity force for the simulation
    ChVector<> gravity = ChVector<>(paramsH->gravity.x, paramsH->gravity.y, paramsH->gravity.z);
    mphysicalSystem.Set_G_acc(gravity);

    /// Set common material Properties
    auto mysurfmaterial = CustomWheelMaterial(ChContactMethod::NSC);//chrono_types::make_shared<ChMaterialSurfaceNSC>();
    // mysurfmaterial->SetFriction(0.9);
    // mysurfmaterial->SetRestitution(0.4);

    /// Create a body for the rigid soil container
    auto box = chrono_types::make_shared<ChBodyEasyBox>(20, 20, 0.02, 1000, false, true, mysurfmaterial);
    box->SetPos(ChVector<>(0, 0, 0));
    box->SetBodyFixed(true);
    mphysicalSystem.Add(box);

    /// Get the initial SPH particle spacing
    double initSpace0 = paramsH->MULT_INITSPACE * paramsH->HSML;

    /// Bottom wall
    ChVector<> size_XY(bxDim / 2 + 3 * initSpace0, byDim / 2 + 3 * initSpace0, 2 * initSpace0);
    ChVector<> pos_zn(0, 0, -3 * initSpace0);
    ChVector<> pos_zp(0, 0, bzDim + 2 * initSpace0);

    /// Left and right wall
    ChVector<> size_YZ(2 * initSpace0, byDim / 2 + 3 * initSpace0, bzDim / 2);
    ChVector<> pos_xp(bxDim / 2 + initSpace0, 0.0, bzDim / 2 + 0 * initSpace0);
    ChVector<> pos_xn(-bxDim / 2 - 3 * initSpace0, 0.0, bzDim / 2 + 0 * initSpace0);

    /// Front and back wall
    ChVector<> size_XZ(bxDim / 2, 2 * initSpace0, bzDim / 2);
    ChVector<> pos_yp(0, byDim / 2 + initSpace0, bzDim / 2 + 0 * initSpace0);
    ChVector<> pos_yn(0, -byDim / 2 - 3 * initSpace0, bzDim / 2 + 0 * initSpace0);

    /// Fluid-Solid Coupling at the walls via BCE particles
    myFsiSystem.AddBceBox(paramsH, box, pos_zn, QUNIT, size_XY, 12);
    myFsiSystem.AddBceBox(paramsH, box, pos_xp, QUNIT, size_YZ, 23);
    myFsiSystem.AddBceBox(paramsH, box, pos_xn, QUNIT, size_YZ, 23);
    myFsiSystem.AddBceBox(paramsH, box, pos_yp, QUNIT, size_XZ, 13);
    myFsiSystem.AddBceBox(paramsH, box, pos_yn, QUNIT, size_XZ, 13);

    driver1 = chrono_types::make_shared<ViperSpeedDriver>(0.1, 0.5 * CH_C_PI);
    // driver2 = chrono_types::make_shared<ViperSpeedDriver>(0.1, 0.5 * CH_C_PI);
    // driver3 = chrono_types::make_shared<ViperSpeedDriver>(0.1, 0.5 * CH_C_PI);
    rover_one = chrono_types::make_shared<Viper>(&mphysicalSystem, ViperWheelType::SimpleWheel);
    // rover_two = chrono_types::make_shared<Viper>(&mphysicalSystem, ViperWheelType::SimpleWheel);
    // rover_three = chrono_types::make_shared<Viper>(&mphysicalSystem, ViperWheelType::SimpleWheel);
    rover_one->SetDriver(driver1);
    rover_one->SetWheelContactMaterial(CustomWheelMaterial(ChContactMethod::NSC));
    rover_one->Initialize(ChFrame<>(ChVector<>(paramsH->bodyIniPosX, paramsH->bodyIniPosY, paramsH->bodyIniPosZ), 
        Q_from_Euler123(ChVector<double>(0, 0, -0.15*CH_C_PI))));
    // rover_two->SetDriver(driver2);
    // rover_two->SetWheelContactMaterial(CustomWheelMaterial(ChContactMethod::NSC));
    // rover_two->Initialize(ChFrame<>(ChVector<>(paramsH->bodyIniPosY, paramsH->bodyIniPosX, paramsH->bodyIniPosZ), 
    //     Q_from_Euler123(ChVector<double>(0, 0, 0.5*CH_C_PI))));
    // rover_three->SetDriver(driver3);
    // rover_three->SetWheelContactMaterial(CustomWheelMaterial(ChContactMethod::NSC));
    // rover_three->Initialize(ChFrame<>(ChVector<>(paramsH->bodyIniPosY, -paramsH->bodyIniPosX, paramsH->bodyIniPosZ), 
    //     Q_from_Euler123(ChVector<double>(0, 0, -0.5*CH_C_PI))));

    /// Add BCE particles and mesh of wheels to the system
    for (int i = 0; i < 4; i++) {
        std::shared_ptr<ChBodyAuxRef> wheel_body;
        if (i == 0) {
            wheel_body = rover_one->GetWheel(ViperWheelID::V_LF)->GetBody();
        }
        if (i == 1) {
            wheel_body = rover_one->GetWheel(ViperWheelID::V_RF)->GetBody();
        }
        if (i == 2) {
            wheel_body = rover_one->GetWheel(ViperWheelID::V_LB)->GetBody();
        }
        if (i == 3) {
            wheel_body = rover_one->GetWheel(ViperWheelID::V_RB)->GetBody();
        }

        myFsiSystem.AddFsiBody(wheel_body);
        std::string BCE_path = GetChronoDataFile("fsi/demo_BCE/BCE_viperWheelSimple.txt");
        if (i == 0 || i == 2) {
            myFsiSystem.AddBceFile(paramsH, wheel_body, BCE_path, ChVector<>(0), Q_from_AngX( CH_C_PI / 2.0), 1.0, true);
        } else {
            myFsiSystem.AddBceFile(paramsH, wheel_body, BCE_path, ChVector<>(0), Q_from_AngX(-CH_C_PI / 2.0), 1.0, true);
        }
    }
    /*for (int i = 0; i < 4; i++) {
        std::shared_ptr<ChBodyAuxRef> wheel_body;
        if (i == 0) {
            wheel_body = rover_two->GetWheel(ViperWheelID::V_LF)->GetBody();
        }
        if (i == 1) {
            wheel_body = rover_two->GetWheel(ViperWheelID::V_RF)->GetBody();
        }
        if (i == 2) {
            wheel_body = rover_two->GetWheel(ViperWheelID::V_LB)->GetBody();
        }
        if (i == 3) {
            wheel_body = rover_two->GetWheel(ViperWheelID::V_RB)->GetBody();
        }

        myFsiSystem.AddFsiBody(wheel_body);
        std::string BCE_path = GetChronoDataFile("fsi/demo_BCE/BCE_viperWheelSimple.txt");
        if (i == 0 || i == 2) {
            myFsiSystem.AddBceFile(paramsH, wheel_body, BCE_path, ChVector<>(0), Q_from_AngX( CH_C_PI / 2.0), 1.0, true);
        } else {
            myFsiSystem.AddBceFile(paramsH, wheel_body, BCE_path, ChVector<>(0), Q_from_AngX(-CH_C_PI / 2.0), 1.0, true);
        }
    }*/
    /*for (int i = 0; i < 4; i++) {
        std::shared_ptr<ChBodyAuxRef> wheel_body;
        if (i == 0) {
            wheel_body = rover_three->GetWheel(ViperWheelID::V_LF)->GetBody();
        }
        if (i == 1) {
            wheel_body = rover_three->GetWheel(ViperWheelID::V_RF)->GetBody();
        }
        if (i == 2) {
            wheel_body = rover_three->GetWheel(ViperWheelID::V_LB)->GetBody();
        }
        if (i == 3) {
            wheel_body = rover_three->GetWheel(ViperWheelID::V_RB)->GetBody();
        }

        myFsiSystem.AddFsiBody(wheel_body);
        std::string BCE_path = GetChronoDataFile("fsi/demo_BCE/BCE_viperWheelSimple.txt");
        if (i == 0 || i == 2) {
            myFsiSystem.AddBceFile(paramsH, wheel_body, BCE_path, ChVector<>(0), Q_from_AngX( CH_C_PI / 2.0), 1.0, true);
        } else {
            myFsiSystem.AddBceFile(paramsH, wheel_body, BCE_path, ChVector<>(0), Q_from_AngX(-CH_C_PI / 2.0), 1.0, true);
        }
    }*/

    /// Add a blade on chassis and create BCE particles
    for (int i = 0; i < 1; i++) {
        std::shared_ptr<ChBodyAuxRef> rover_body;
        std::string blade_name;
        if (i == 0){
            rover_body = rover_one->GetChassis()->GetBody();
            blade_name = "blade_body_one";
        }
        // if (i == 1){
        //     rover_body = rover_two->GetChassis()->GetBody();
        //     blade_name = "blade_body_two";
        // }
        // if (i == 2){
        //     rover_body = rover_three->GetChassis()->GetBody();
        //     blade_name = "blade_body_three";
        // }
        ChVector<> rover_body_pos = rover_body->GetPos();

        // Load mesh from obj file
        auto trimesh = chrono_types::make_shared<ChTriangleMeshConnected>();
        std::string obj_path = "./blade.obj";
        trimesh->LoadWavefrontMesh(obj_path, true, true);
        double scale_ratio = 1.0;
        // trimesh->Transform(ChVector<>(0, 0, 0), ChMatrix33<>(body_rot));       // rotate the mesh if needed
        // trimesh->Transform(ChVector<>(0, 0, 0), ChMatrix33<>(scale_ratio));     // scale to a different size
        // trimesh->RepairDuplicateVertexes(1e-9);                                 // if meshes are not watertight  

        // Compute mass inertia from mesh
        double mmass = 1.0;
        double mdensity = 1.0; //paramsH->bodyDensity;
        ChVector<> mcog = ChVector<>(0.5, 0.0, -0.5);
        // ChMatrix33<> minertia;
        // trimesh->ComputeMassProperties(true, mmass, mcog, minertia);
        ChVector<> principal_I = ChVector<>(1.0, 1.0, 1.0);
        ChMatrix33<> principal_inertia_rot = ChMatrix33<>(1.0);
        // ChInertiaUtils::PrincipalInertia(minertia, principal_I, principal_inertia_rot);

        // Set the abs orientation, position
        auto blade_body = chrono_types::make_shared<ChBodyAuxRef>();
        blade_body->SetNameString(blade_name);
        double rot_ang_x, rot_ang_y, rot_ang_z;
        ChVector<> blade_rel_pos; 
        if (i == 0){
            blade_rel_pos = ChVector<>(1.8, 0.0, -0.47);
            rot_ang_x = 0.0 / 180.0 * CH_C_PI;
            rot_ang_y =-3.0 / 180.0 * CH_C_PI;
            rot_ang_z = 0.0 / 180.0 * CH_C_PI;
        }
        // if (i == 1){
        //     blade_rel_pos = ChVector<>(0.0, 1.2, -0.58);
        //     rot_ang_x = 0.0 / 180.0 * CH_C_PI;
        //     rot_ang_y =-30.0 / 180.0 * CH_C_PI;
        //     rot_ang_z = 90.0 / 180.0 * CH_C_PI;
        // }
        // if (i == 2){
        //     blade_rel_pos = ChVector<>(0.0, -1.2, -0.58);
        //     rot_ang_x = 0.0 / 180.0 * CH_C_PI;
        //     rot_ang_y =-30.0 / 180.0 * CH_C_PI;
        //     rot_ang_z =-90.0 / 180.0 * CH_C_PI;
        // }
        ChVector<> blade_pos = rover_body_pos + blade_rel_pos;
        ChQuaternion<> blade_rot = Q_from_Euler123(ChVector<double>(rot_ang_x, rot_ang_y, rot_ang_z));

        // Set the COG coordinates to barycenter, without displacing the REF reference.
        // Make the COG frame a principal frame.
        blade_body->SetFrame_COG_to_REF(ChFrame<>(mcog, principal_inertia_rot));

        // Set inertia
        std::cout << "\n" << "The mass of the blade is " << mmass * mdensity << "\n" << std::endl;
        blade_body->SetMass(mmass * mdensity);
        blade_body->SetInertiaXX(mdensity * principal_I);
        
        // Set the absolute position of the body:
        blade_body->SetFrame_REF_to_abs(ChFrame<>(ChVector<>(blade_pos),ChQuaternion<>(blade_rot)));                              
        mphysicalSystem.Add(blade_body);

        blade_body->SetBodyFixed(false);
        blade_body->GetCollisionModel()->ClearModel();
        blade_body->GetCollisionModel()->AddTriangleMesh(mysurfmaterial, trimesh, false, false, VNULL, ChMatrix33<>(1), 0.005);
        blade_body->GetCollisionModel()->BuildModel();
        blade_body->SetCollide(false);

        // Fix the blade on the chassis
        auto fix_link = chrono_types::make_shared<ChLinkLockLock>(); 
        ChVector<> link_pos = blade_pos;
        fix_link->Initialize(rover_body, blade_body, ChCoordsys<>(link_pos, QUNIT));
        mphysicalSystem.AddLink(fix_link);

        // Create BCE particles associated with mesh
        // std::vector<ChVector<>> BCE_par;
        // CreateMeshMarkers(trimesh, (double)initSpace0, BCE_par);
        // myFsiSystem.AddFsiBody(blade_body);
        // myFsiSystem.AddBceFromPoints(paramsH, blade_body, BCE_par, ChVector<>(0.0), QUNIT);
    }

    /// Add some rocks on the terrain
    std::vector<ChVector<>> BCE_par_rock;
    int n_r = 0;
    for (int i = 0; i < 0; i++) {
    for (int j = 0; j < 0; j++) {
        std::string rock_name = "rock" + std::to_string(n_r+1);
        // Load mesh from obj file
        auto trimesh = chrono_types::make_shared<ChTriangleMeshConnected>();
        std::string obj_path = "./rock3.obj";
        trimesh->LoadWavefrontMesh(obj_path, true, true);
        double scale_ratio = rock_scale;
        // trimesh->Transform(ChVector<>(0, 0, 0), ChMatrix33<>(body_rot));       // rotate the mesh if needed
        trimesh->Transform(ChVector<>(0, 0, 0), ChMatrix33<>(scale_ratio));    // scale to a different size
        // trimesh->RepairDuplicateVertexes(1e-9);                                // if meshes are not watertight  

        // Compute mass inertia from mesh
        double mmass;// = 5.0;
        double mdensity = paramsH->bodyDensity;
        ChVector<> mcog;// = ChVector<>(0.0, 0.0, 0.0);
        ChMatrix33<> minertia;
        trimesh->ComputeMassProperties(true, mmass, mcog, minertia);
        ChVector<> principal_I;// = ChVector<>(1.0, 1.0, 1.0);
        ChMatrix33<> principal_inertia_rot;// = ChMatrix33<>(1.0);
        ChInertiaUtils::PrincipalInertia(minertia, principal_I, principal_inertia_rot);

        // Set the abs orientation, position
        auto rock_body = chrono_types::make_shared<ChBodyAuxRef>();
        rock_body->SetNameString(rock_name);
        double rot_ang_x, rot_ang_y, rot_ang_z;
        ChVector<> rock_rel_pos; 
        
        // Set initial pos and rot
        rot_ang_x = i * 30.0 / 180.0 * CH_C_PI;
        rot_ang_y = j * 45.0 / 180.0 * CH_C_PI;
        rot_ang_z = i * 60.0 / 180.0 * CH_C_PI;
        double det_x = 0.15 * pow(-1.0, i + j);
        double det_y = 0.15 * pow(-1.0, i + j + 1);
        ChVector<> rock_pos = ChVector<>(-2.5 + i * 1.25 + det_x, -2.5 + j * 1.25 + det_y, 0.6);
        ChQuaternion<> rock_rot = Q_from_Euler123(ChVector<double>(rot_ang_x, rot_ang_y, rot_ang_z));

        // Set the COG coordinates to barycenter, without displacing the REF reference.
        // Make the COG frame a principal frame.
        rock_body->SetFrame_COG_to_REF(ChFrame<>(mcog, principal_inertia_rot));

        // Set inertia
        std::cout << "\n" << "The mass of the rock is " << mmass * mdensity << "\n" << std::endl;
        rock_body->SetMass(mmass * mdensity);
        rock_body->SetInertiaXX(mdensity * principal_I);
        
        // Set the absolute position of the body:
        rock_body->SetFrame_REF_to_abs(ChFrame<>(ChVector<>(rock_pos),ChQuaternion<>(rock_rot)));                              
        mphysicalSystem.Add(rock_body);

        // Set collision
        rock_body->SetBodyFixed(false);
        rock_body->GetCollisionModel()->ClearModel();
        rock_body->GetCollisionModel()->AddTriangleMesh(mysurfmaterial, trimesh, false, false, VNULL, ChMatrix33<>(1), 0.005);
        rock_body->GetCollisionModel()->BuildModel();
        rock_body->SetCollide(true);

        // Create BCE particles associated with mesh
        if(i==0){
            CreateMeshMarkers(trimesh, (double)initSpace0, BCE_par_rock);
        }
        myFsiSystem.AddFsiBody(rock_body);
        myFsiSystem.AddBceFromPoints(paramsH, rock_body, BCE_par_rock, ChVector<>(0.0), QUNIT);
        n_r++;
    }
    }
}

//------------------------------------------------------------------
// Function to save the povray files of the MBD
//------------------------------------------------------------------
void SaveParaViewFiles(ChSystemFsi& myFsiSystem,
                       ChSystemNSC& mphysicalSystem,
                       std::shared_ptr<fsi::SimParams> paramsH,
                       int next_frame,
                       double mTime) {
    double frame_time = 1.0 / paramsH->out_fps;
    char filename[4096];
    std::string objextension = ".obj";
    std::string vtkextension = ".vtk";
    std::string partname = "";

    if (pv_output && std::abs(mTime - (next_frame)*frame_time) < 1e-5) {
        /// save the SPH particles
        myFsiSystem.PrintParticleToFile(demo_dir);

        std::shared_ptr<Viper> rover;
        std::shared_ptr<ChBody> body;
        std::string obj_path = "";
        int n_body = 0;
        for (int n = 0; n < 1; n++) {
            /// pick the rover
            if (n==0)
                rover = rover_one;
            // if (n==1)
            //     rover = rover_two;
            // if (n==2)
            //     rover = rover_three;

            /// pick the body on the rover
            for (int i = 0; i < 18 + 0; i++) {
                if (i==0){
                    body = rover->GetChassis()->GetBody();
                    obj_path = (GetChronoDataFile("robot/viper/obj/viper_chassis.obj"));
                    partname = "body_" + std::to_string(n+1) + "_";
                }
                if (i==1){
                    body = rover->GetWheel(ViperWheelID::V_LF)->GetBody();
                    obj_path = GetChronoDataFile("robot/viper/obj/viper_simplewheel.obj");
                    partname = "wheel_" + std::to_string(n+1) + "_1_";
                }
                if (i==2){
                    body = rover->GetWheel(ViperWheelID::V_RF)->GetBody();
                    obj_path = GetChronoDataFile("robot/viper/obj/viper_simplewheel.obj");
                    partname = "wheel_" + std::to_string(n+1) + "_2_";
                }
                if (i==3){
                    body = rover->GetWheel(ViperWheelID::V_LB)->GetBody();
                    obj_path = GetChronoDataFile("robot/viper/obj/viper_simplewheel.obj");
                    partname = "wheel_" + std::to_string(n+1) + "_3_";
                }
                if (i==4){
                    body = rover->GetWheel(ViperWheelID::V_RB)->GetBody();
                    obj_path = GetChronoDataFile("robot/viper/obj/viper_simplewheel.obj");
                    partname = "wheel_" + std::to_string(n+1) + "_4_";
                }
                if (i==5){
                    body = rover->GetUpright(ViperWheelID::V_LF)->GetBody();
                    obj_path = GetChronoDataFile("robot/viper/obj/viper_L_steer.obj");
                    partname = "steerRod_" + std::to_string(n+1) + "_1_";
                }
                if (i==6){
                    body = rover->GetUpright(ViperWheelID::V_RF)->GetBody();
                    obj_path = GetChronoDataFile("robot/viper/obj/viper_R_steer.obj");
                    partname = "steerRod_" + std::to_string(n+1) + "_2_";
                }
                if (i==7){
                    body = rover->GetUpright(ViperWheelID::V_LB)->GetBody();
                    obj_path = GetChronoDataFile("robot/viper/obj/viper_L_steer.obj");
                    partname = "steerRod_" + std::to_string(n+1) + "_3_";
                }
                if (i==8){
                    body = rover->GetUpright(ViperWheelID::V_RB)->GetBody();
                    obj_path = GetChronoDataFile("robot/viper/obj/viper_R_steer.obj");
                    partname = "steerRod_" + std::to_string(n+1) + "_4_";
                }
                if (i==9){
                    body = rover->GetLowerArm(ViperWheelID::V_LF)->GetBody();
                    obj_path = GetChronoDataFile("robot/viper/obj/viper_L_bt_sus.obj");
                    partname = "lowerRod_" + std::to_string(n+1) + "_1_";
                }
                if (i==10){
                    body = rover->GetLowerArm(ViperWheelID::V_RF)->GetBody();
                    obj_path = GetChronoDataFile("robot/viper/obj/viper_R_bt_sus.obj");
                    partname = "lowerRod_" + std::to_string(n+1) + "_2_";
                }
                if (i==11){
                    body = rover->GetLowerArm(ViperWheelID::V_LB)->GetBody();
                    obj_path = GetChronoDataFile("robot/viper/obj/viper_L_bt_sus.obj");
                    partname = "lowerRod_" + std::to_string(n+1) + "_3_";
                }
                if (i==12){
                    body = rover->GetLowerArm(ViperWheelID::V_RB)->GetBody();
                    obj_path = GetChronoDataFile("robot/viper/obj/viper_R_bt_sus.obj");
                    partname = "lowerRod_" + std::to_string(n+1) + "_4_";
                }
                if (i==13){
                    body = rover->GetUpperArm(ViperWheelID::V_LF)->GetBody();
                    obj_path = GetChronoDataFile("robot/viper/obj/viper_L_up_sus.obj");
                    partname = "upperRod_" + std::to_string(n+1) + "_1_";
                }
                if (i==14){
                    body = rover->GetUpperArm(ViperWheelID::V_RF)->GetBody();
                    obj_path = GetChronoDataFile("robot/viper/obj/viper_R_up_sus.obj");
                    partname = "upperRod_" + std::to_string(n+1) + "_2_";
                }
                if (i==15){
                    body = rover->GetUpperArm(ViperWheelID::V_LB)->GetBody();
                    obj_path = GetChronoDataFile("robot/viper/obj/viper_L_up_sus.obj");
                    partname = "upperRod_" + std::to_string(n+1) + "_3_";
                }
                if (i==16){
                    body = rover->GetUpperArm(ViperWheelID::V_RB)->GetBody();
                    obj_path = GetChronoDataFile("robot/viper/obj/viper_R_up_sus.obj");
                    partname = "upperRod_" + std::to_string(n+1) + "_4_";
                }
                if (i==17){
                    if (n==0)
                        body = mphysicalSystem.SearchBody("blade_body_one");
                    // if (n==1)
                    //     body = mphysicalSystem.SearchBody("blade_body_two");
                    // if (n==2)
                    //     body = mphysicalSystem.SearchBody("blade_body_three");
                    obj_path = ("./blade.obj");
                    partname = "blade_" + std::to_string(n+1) + "_";
                }
                if (i > 17){
                    std::string rockname = "rock" + std::to_string(i-17);
                    body = mphysicalSystem.SearchBody(rockname.c_str());
                    obj_path = ("./rock3.obj");
                    partname = "rock_" + std::to_string(i-17) + "_";
                }

                /// ============== save mesh file ===================
                ChFrame<> body_ref_frame = body->GetFrame_REF_to_abs();
                ChVector<> pos = body_ref_frame.GetPos();
                ChQuaternion<> rot = body_ref_frame.GetRot();
                ChVector<> vel = body->GetPos_dt();
                if (i == 1 || i == 3) {
                    rot.Cross(rot, Q_from_AngZ(CH_C_PI));
                }

                auto mmesh = chrono_types::make_shared<ChTriangleMeshConnected>();
                double scale_ratio = 1.0;
                if (i > 17){
                    scale_ratio = rock_scale;
                }
                mmesh->LoadWavefrontMesh(obj_path, true, true);
                mmesh->Transform(ChVector<>(0, 0, 0), ChMatrix33<>(scale_ratio));  // scale to a different size
                // mmesh->RepairDuplicateVertexes(1e-9);                              // if meshes are not watertight
                mmesh->Transform(pos, ChMatrix33<>(rot));  // rotate the mesh based on the orientation of body

                if (save_obj) {  // save to obj file
                    sprintf(filename, "%s/%s%d%s", paramsH->demo_dir, partname.c_str(), next_frame, objextension.c_str());
                    std::vector<geometry::ChTriangleMeshConnected> meshes = {*mmesh};
                    geometry::ChTriangleMeshConnected::WriteWavefront(filename, meshes);
                } 
                if (save_vtk) {  // save to vtk file
                    sprintf(filename, "%s/%s%d%s", paramsH->demo_dir, partname.c_str(), next_frame, vtkextension.c_str());
                    std::ofstream file;
                    file.open(filename);
                    file << "# vtk DataFile Version 2.0" << std::endl;
                    file << "VTK from simulation" << std::endl;
                    file << "ASCII" << std::endl;
                    file << "DATASET UNSTRUCTURED_GRID" << std::endl;
                    auto nv = mmesh->getCoordsVertices().size();
                    file << "POINTS " << nv << " float" << std::endl;
                    for (auto& v : mmesh->getCoordsVertices())
                        file << v.x() << " " << v.y() << " " << v.z() << std::endl;
                    auto nf = mmesh->getIndicesVertexes().size();
                    file << "CELLS " << nf << " " << 4 * nf << std::endl;
                    for (auto& f : mmesh->getIndicesVertexes())
                        file << "3 " << f.x() << " " << f.y() << " " << f.z() << std::endl;
                    file << "CELL_TYPES " << nf << std::endl;
                    for (size_t ii = 0; ii < nf; ii++)
                        file << "5 " << std::endl;
                    file.close();
                }

                /// save rigid body position and rotation
                std::string delim = ",";
                sprintf(filename, "%s/body_pos_rot_vel%d.csv", paramsH->demo_dir, n_body + 1);
                std::ofstream file;
                if (mphysicalSystem.GetChTime() > 0)
                    file.open(filename, std::fstream::app);
                else {
                    file.open(filename);
                    file << "Time" << delim << "x" << delim << "y" << delim << "z" << delim << "q0" << delim << "q1"
                        << delim << "q2" << delim << "q3" << delim << "Vx" << delim << "Vy" << delim << "Vz" << std::endl;
                }

                file << mphysicalSystem.GetChTime() << delim << pos.x() << delim << pos.y() << delim << pos.z() << delim
                    << rot.e0() << delim << rot.e1() << delim << rot.e2() << delim << rot.e3() << delim << vel.x() << delim
                    << vel.y() << delim << vel.z() << std::endl;

                file.close();

                n_body = n_body + 1;
            }
        }

        std::cout << "-------------------------------------\n" << std::endl;
        std::cout << "             Output frame:   " << next_frame << std::endl;
        std::cout << "             Time:           " << mTime << std::endl;
        std::cout << "-------------------------------------\n" << std::endl;
    }
}
