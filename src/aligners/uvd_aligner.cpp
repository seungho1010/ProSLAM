#include "uvd_aligner.h"

#include "types/landmark.h"

namespace proslam {
  using namespace srrg_core;

  //ds initialize aligner with minimal entity
  void UVDAligner::initialize(Frame* frame_, const TransformMatrix3D& robot_to_world_) {
    _frame = frame_;
    _errors.resize(_frame->activePoints().size());
    _inliers.resize(_frame->activePoints().size());
    _robot_to_world = robot_to_world_;
    _world_to_robot = _robot_to_world.inverse();

    //ds wrappers for optimization
    _camera_to_world = _robot_to_world*_frame->cameraLeft()->cameraToRobot();
    _world_to_camera = _camera_to_world.inverse();
    _camera_matrix  = _frame->cameraLeft()->cameraMatrix();
    _number_of_rows_image = _frame->cameraLeft()->imageRows();
    _number_of_cols_image = _frame->cameraLeft()->imageCols();
  }

  //ds linearize the system: to be called inside oneRound
  void UVDAligner::linearize(const bool& ignore_outliers_) {

    //ds initialize setup
    _H.setZero();
    _b.setZero();
    _number_of_inliers  = 0;
    _number_of_outliers = 0;
    _total_error        = 0;

    //ds loop over all points (assumed to have previous points)
    for (Index index_point = 0; index_point < _frame->activePoints().size(); index_point++) {
      _errors[index_point]  = -1;
      _inliers[index_point] = false;
      _omega.setIdentity();
      _omega(2,2)*=10;
      
      //ds buffer framepoint
      FramePoint* frame_point = _frame->activePoints()[index_point];
      assert(_frame->cameraLeft()->isInFieldOfView(frame_point->imageCoordinatesLeft()));
      assert(frame_point->previous());

      //ds buffer landmark
      const Landmark* landmark = frame_point->landmark();

      //ds compute the point in the camera frame - prefering a landmark estimate if available
      PointCoordinates predicted_point_in_camera = PointCoordinates::Zero();
      if (landmark && landmark->areCoordinatesValidated()) {
        predicted_point_in_camera = _world_to_camera*landmark->coordinates();
      } else {
        predicted_point_in_camera = _world_to_camera*frame_point->previous()->worldCoordinates();
        _omega *= _weight_framepoint;
      }
      const real& depth_meters = predicted_point_in_camera.z();
      if (depth_meters <= 0 || depth_meters > _maximum_depth_far_meters) {
        continue;
      }

      //ds retrieve homogeneous projections
      const PointCoordinates predicted_uvd_in_camera  = _camera_matrix*predicted_point_in_camera;
      
      //ds compute the image coordinates
      PointCoordinates predicted_point_in_image  = predicted_uvd_in_camera/depth_meters;
      //restore the depth in the third component
      predicted_point_in_image.z()=depth_meters;
      
      //ds if the point is outside the image, skip
      if (predicted_point_in_image.x() < 0 || predicted_point_in_image.x() > _number_of_cols_image||
          predicted_point_in_image.y() < 0 || predicted_point_in_image.y() > _number_of_rows_image) {
        continue;
      }

      assert(_frame->cameraLeft()->isInFieldOfView(predicted_point_in_image));
      
      //ds precompute
      const real inverse_predicted_d  = 1/depth_meters;
      const real inverse_predicted_d_squared  = inverse_predicted_d*inverse_predicted_d;

      //ds visualization only
      frame_point->setReprojectionCoordinatesLeft(predicted_point_in_image);

      //ds compute error
      const Vector3 error(predicted_point_in_image.x()-frame_point->imageCoordinatesLeft().x(),
                          predicted_point_in_image.y()-frame_point->imageCoordinatesLeft().y(),
			  predicted_point_in_image.z()-frame_point->cameraCoordinatesLeft().z());

      //ds compute squared error
      const real chi = error.transpose()*error;

      //ds update error stats
      _errors[index_point] = chi;

      //ds if outlier
      if (chi > _maximum_error_kernel) {
        ++_number_of_outliers;
        if (ignore_outliers_) {
          continue;
        }

        //ds include kernel in omega
        _omega *= _maximum_error_kernel/chi;
      } else {
        _inliers[index_point] = true;
        ++_number_of_inliers;
      }

      //ds update total error
      _total_error += _errors[index_point];

      //ds compute the jacobian of the transformation
      Matrix3_6 jacobian_transform;
      jacobian_transform.setZero();

      //ds if the point is near enough - we consider translation
      if (depth_meters < _maximum_depth_near_meters) {
        jacobian_transform.block<3,3>(0,0).setIdentity();
      }

      //ds always consider rotation
      jacobian_transform.block<3,3>(0,3) = -2*skew(predicted_point_in_camera);

      //ds jacobian parts of the homogeneous division
      Matrix3 jacobian_projection;
      jacobian_projection <<
      inverse_predicted_d, 0, -predicted_uvd_in_camera.x()*inverse_predicted_d_squared,
      0, inverse_predicted_d, -predicted_uvd_in_camera.y()*inverse_predicted_d_squared,
      0, 0, 1;

      //ds assemble final jacobian
      _jacobian = jacobian_projection*_camera_matrix*jacobian_transform;

      //ds precompute transposed
      const Matrix6_3 jacobian_transposed(_jacobian.transpose());

      if (depth_meters < _maximum_depth_near_meters) {
        _omega *= (_maximum_depth_near_meters-depth_meters)/_maximum_depth_near_meters;
      } else {
        _omega *= (_maximum_depth_far_meters-depth_meters)/_maximum_depth_far_meters;
      }

      //ds update H and b
      _H += jacobian_transposed*_omega*_jacobian;
      _b += jacobian_transposed*_omega*error;
    }
  }

  //ds solve alignment problem for one round
  void UVDAligner::oneRound(const bool& ignore_outliers_) {

    //ds linearize system once
    linearize(ignore_outliers_);

    //ds always damp
    _H += _damping*Matrix6::Identity();

    //ds compute solution transformation
    const Vector6 dx = _H.ldlt().solve(-_b);
    _world_to_camera = v2t(dx)*_world_to_camera;

    //ds enforce proper rotation matrix
    const Matrix3 rotation = _world_to_camera.linear();
    Matrix3 rotation_squared             = rotation.transpose() * rotation;
    rotation_squared.diagonal().array() -= 1;
    _world_to_camera.linear()      -= 0.5*rotation*rotation_squared;
  }

  //ds solve alignment problem until convergence is reached
  void UVDAligner::converge() {

    //ds previous error to check for convergence
    real total_error_previous = 0;

    //ds start LS
    for (Count iteration = 0; iteration < _maximum_number_of_iterations; ++iteration) {
      oneRound(false);

      //ds check if converged (no descent required)
      if (_error_delta_for_convergence > std::fabs(total_error_previous-_total_error)) {

        //ds trigger inlier only runs
        oneRound(true);
        oneRound(true);
        oneRound(true);

        //ds compute information matrix
        _information_matrix = _H;

        //ds system converged
        _has_system_converged = true;
        break;
      } else {
        total_error_previous = _total_error;
      }

      //ds check last iteration
      if(iteration == _maximum_number_of_iterations-1) {
        _has_system_converged = false;
        LOG_INFO(std::cerr << "UVDAligner::converge|WARNING: system did not converge - total error: "  << _total_error
                  << " average error: " << _total_error/(_number_of_inliers+_number_of_outliers)
                  << " inliers: " << _number_of_inliers << " outliers: " << _number_of_outliers << std::endl)
      }
    }

    //ds update wrapped structures
    _camera_to_world = _world_to_camera.inverse();    
    _robot_to_world = _camera_to_world*_frame->cameraLeft()->robotToCamera();
    _world_to_robot = _robot_to_world.inverse();
  }
}
