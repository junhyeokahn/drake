#pragma once

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <Eigen/Dense>

#include "drake/common/drake_copyable.h"
#include "drake/geometry/dev/query_object.h"
#include "drake/geometry/dev/render/camera_properties.h"
#include "drake/geometry/geometry_ids.h"
#include "drake/systems/framework/diagram.h"
#include "drake/systems/framework/leaf_system.h"
#include "drake/systems/rendering/pose_vector.h"
#include "drake/systems/sensors/camera_info.h"
#include "drake/systems/sensors/image.h"

namespace drake {
namespace systems {
namespace sensors {
namespace dev {
/** An RGB-D camera system that provides RGB, depth, and label images using
 the geometry in the geometry::dev::SceneGraph.

 @system{RgbdCamera,
    @input_port{geometry_query},
    @output_port{color_image} @output_port{depth_image} @output_port{label_image} @output_port{X_WB}
 }

 Let `W` be the world coordinate system. In addition to `W`, there are three
 more coordinate systems that are associated with an RgbdCamera2. They are
 defined as follows:

   * `B` - the camera's base coordinate system: X-forward, Y-left, and Z-up.

   * `C` - the camera's color sensor's optical coordinate system: `X-right`,
           `Y-down` and `Z-forward`.

   * `D` - the camera's depth sensor's optical coordinate system: `X-right`,
           `Y-down` and `Z-forward`.

 Frames `C` and `D` are coincident and aligned. The origins of `C` and `D`
 (`Co` and `Do`, respectively) have position
 `p_BoCo_B = p_BoDo_B = <0 m, 0.02 m, 0 m>`. In other words `X_CD = I`.
 This definition implies that the depth image is a "registered depth image"
 for the RGB image. No disparity between the RGB and depth images are
 modeled in this system. For more details about the poses of `C` and `D`,
 see the class documentation of CameraInfo.

 Output image format:
   - The RGB image has four channels in the following order: red, green
     blue, and alpha. Each channel is represented by a uint8_t.

   - The depth image has a depth channel represented by a float. The value
     stored in the depth channel holds *the Z value in `D`.*  Note that this
     is different from the range data used by laser range finders (like that
     provided by DepthSensor) in which the depth value represents the
     distance from the sensor origin to the object's surface.

   - The label image has a single channel represented by a int16_t. The value
     stored in the channel holds a model ID which corresponds to an object
     in the scene. For the pixels corresponding to no body, namely the sky
     and the flat terrain, we assign RenderLabel::empty_label() and
     RenderLabel::terrain_label(), respectively.
     <!-- TODO(SeanCurtis-TRI): Update these names based on fixing labels. -->

 @ingroup sensor_systems  */
class RgbdCamera final : public LeafSystem<double> {
 public:
  DRAKE_NO_COPY_NO_MOVE_NO_ASSIGN(RgbdCamera)

  // TODO(SeanCurtis-TRI): Move this into a generic utility/system. After all,
  // it just needs to take an input image and produce a point cloud.
  /** Converts a depth image obtained from RgbdCamera to a point cloud.  If a
   pixel in the depth image has NaN depth value, all the `(x, y, z)` values
   in the converted point will be NaN.
   Similarly, if a pixel has either InvalidDepth::kTooFar or
   InvalidDepth::kTooClose, the converted point will be
   InvalidDepth::kTooFar. Note that this matches the convention used by the
   Point Cloud Library (PCL).
  
   @param[in] depth_image The input depth image obtained from RgbdCamera.
   @param[in] camera_info The input camera info which is used for conversion.
   @param[out] point_cloud The pointer of output point cloud.  */
  // TODO(kunimatsu-tri) Use drake::perception::PointCloud instead of
  // Eigen::Matrix3Xf and create new constants there instead of reusing
  // InvalidDepth.
  // TODO(kunimatsu-tri) Move this to drake::perception.
  static void ConvertDepthImageToPointCloud(
      const ImageDepth32F& depth_image,
      const CameraInfo& camera_info, Eigen::Matrix3Xf* point_cloud);

  /** Constructs an %RgbdCamera that is fixed to the world frame with the given
   pose and given camera properties.
  
   @param name           The name of the RgbdCamera. This can be any value, but
                         typically should be unique among all sensors attached
                         to a particular model instance.
   @param p_WB_W         The position of frame `B`'s origin relative to the
                         world origin, expressed in the world frame.
   @param rpy_WB_W       The roll-pitch-yaw orientation of `B` in `W`. This
                         defines `R_WB_W`, the orientation component of `X_WB`.
   @param properties     The camera properties which define this camera's
                         intrinsics.
   @param show_window    A flag for showing a visible window. If this is false,
                         off-screen rendering is executed. This is useful for
                         debugging purposes. The default is false.  */
  RgbdCamera(const std::string& name,
              const Eigen::Vector3d& p_WB_W,
              const Eigen::Vector3d& rpy_WB_W,
              const geometry::dev::render::DepthCameraProperties& properties,
              bool show_window = false);

  /** Constructs an %RgbdCamera whose frame `B` is the frame indicated by
   `frame_id` and the given camera properties. The camera will move as frame
   `B` moves.

   @param name           The name of the RgbdCamera. This can be any value, but
                         typically should be unique among all sensors attached
                         to a particular model instance.
   @param frame_id       The identifier of a parent frame `P` in
                         geometry::dev::SceneGraph to which this camera is
                         rigidly affixed with pose `X_PB`.
   @param X_PB           The pose the camera `B` frame relative to the parent
                         frame `P`.
   @param properties     The camera properties which define this camera's
                         intrinsics.
   @param show_window    A flag for showing a visible window. If this is false,
                         off-screen rendering is executed. This is useful for
                         debugging purposes. The default is false.  */
  RgbdCamera(const std::string& name,
              geometry::FrameId parent_frame,
              const Isometry3<double>& X_PB,
              const geometry::dev::render::DepthCameraProperties& properties,
              bool show_window = false);

  ~RgbdCamera() = default;

  /** Returns the color sensor's info.  */
  const CameraInfo& color_camera_info() const { return color_camera_info_; }

  /** Returns the depth sensor's info.  */
  const CameraInfo& depth_camera_info() const { return depth_camera_info_; }

  /** Returns `X_BC`.  */
  const Eigen::Isometry3d& color_camera_optical_pose() const { return X_BC_; }

  /** Returns `X_BD`.  */
  const Eigen::Isometry3d& depth_camera_optical_pose() const { return X_BD_; }

  /** Returns the id of the frame to which this camera is affixed. */
  geometry::FrameId parent_frame_id() const { return parent_frame_; }

  /** Returns the geometry::dev::QueryObject-valued input port. */
  const InputPort<double>& query_object_input_port() const;

  /** Returns the abstract valued output port that contains an RGBA image of the
   type ImageRgba8U.  */
  const OutputPort<double>& color_image_output_port() const;

  /** Returns the abstract valued output port that contains an ImageDepth32F. */
  const OutputPort<double>& depth_image_output_port() const;

  /** Returns the abstract valued output port that contains an label image of
   the type ImageLabel16I.  */
  const OutputPort<double>& label_image_output_port() const;

  /** Returns the vector valued output port that contains a PoseVector.  */
  const OutputPort<double>& camera_base_pose_output_port() const;

 private:
  void InitPorts(const std::string& name);

  // These are the calculator methods for the four output ports.
  void CalcColorImage(const Context<double>& context,
                      ImageRgba8U* color_image) const;
  void CalcDepthImage(const Context<double>& context,
                      ImageDepth32F* depth_image) const;
  void CalcLabelImage(const Context<double>& context,
                      ImageLabel16I* label_image) const;
  void CalcPoseVector(const Context<double>& context,
                      rendering::PoseVector<double>* pose_vector) const;

  const geometry::dev::QueryObject<double>& get_query_object(
      const Context<double>& context) const {
    return this
        ->EvalAbstractInput(context, query_object_input_port_->get_index())
        ->template GetValue<geometry::dev::QueryObject<double>>();
  }

  const InputPort<double>* query_object_input_port_{};
  const OutputPort<double>* color_image_port_{};
  const OutputPort<double>* depth_image_port_{};
  const OutputPort<double>* label_image_port_{};
  const OutputPort<double>* camera_base_pose_port_{};

  // The identifier for the parent frame `P`.
  const geometry::FrameId parent_frame_;

  // If true, a window will be shown for the camera.
  const bool show_window_;
  const CameraInfo color_camera_info_;
  const CameraInfo depth_camera_info_;
  const geometry::dev::render::DepthCameraProperties properties_;
  // The position of the camera's B frame relative to its parent frame P.
  const Eigen::Isometry3d X_PB_;

  // The color sensor's origin (`Co`) is offset by 0.02 m on the Y axis of
  // the RgbdCamera's base coordinate system (`B`).
  const Eigen::Isometry3d X_BC_{Eigen::Translation3d(0., 0.02, 0.) *
      (Eigen::AngleAxisd(-M_PI_2, Eigen::Vector3d::UnitX()) *
          Eigen::AngleAxisd(M_PI_2, Eigen::Vector3d::UnitY()))};

  // TODO(kunimatsu-tri) Change the X_BD_ to be different from X_BC_ when
  // it's needed.
  // The depth sensor's origin (`Do`) is offset by 0.02 m on the Y axis of
  // the RgbdCamera's base coordinate system (`B`).
  const Eigen::Isometry3d X_BD_{Eigen::Translation3d(0., 0.02, 0.) *
      (Eigen::AngleAxisd(-M_PI_2, Eigen::Vector3d::UnitX()) *
          Eigen::AngleAxisd(M_PI_2, Eigen::Vector3d::UnitY()))};
};

/**
 * Wraps a continuous RgbdCamera with zero order holds to have it function as
 * a discrete sensor.

 @system{RgbdCameraDiscrete,
    @input_port{geometry_query},
    @output_port{color_image} @output_port{depth_image} @output_port{label_image} @output_port{X_WB}
 }
 */
class RgbdCameraDiscrete final : public systems::Diagram<double> {
 public:
  DRAKE_NO_COPY_NO_MOVE_NO_ASSIGN(RgbdCameraDiscrete);

  static constexpr double kDefaultPeriod = 1. / 30;

  /// Constructs a diagram containing a (non-registered) RgbdCamera that will
  /// update at a given rate.
  /// @param period
  ///   Update period (sec).
  /// @param render_label_image
  ///   If true, renders label image (which requires additional overhead). If
  ///   false, `label_image_output_port` will raise an error if called.
  RgbdCameraDiscrete(std::unique_ptr<RgbdCamera> camera,
                      double period = kDefaultPeriod,
                      bool render_label_image = true);

  /// Returns reference to RgbdCamera instance.
  const RgbdCamera& camera() const { return *camera_; }

  /// Returns reference to RgbdCamera instance.
  RgbdCamera& mutable_camera() { return *camera_; }

  /// Returns update period for discrete camera.
  double period() const { return period_; }

  /// @see RgbdCamera::query_object_input_port().
  const InputPort<double>& query_object_input_port() const {
    return get_input_port(query_object_port_);
  }

  /// @see RgbdCamera::color_image_output_port().
  const systems::OutputPort<double>& color_image_output_port() const {
    return get_output_port(output_port_color_image_);
  }

  /// @see RgbdCamera::depth_image_output_port().
  const systems::OutputPort<double>& depth_image_output_port() const {
    return get_output_port(output_port_depth_image_);
  }

  /// @see RgbdCamera::label_image_output_port().
  const systems::OutputPort<double>& label_image_output_port() const {
    return get_output_port(output_port_label_image_);
  }

  /// @see RgbdCamera::camera_base_pose_output_port().
  const systems::OutputPort<double>& camera_base_pose_output_port() const {
    return get_output_port(output_port_pose_);
  }

 private:
  RgbdCamera* const camera_{};
  const double period_{};

  int query_object_port_{-1};
  int output_port_color_image_{-1};
  int output_port_depth_image_{-1};
  int output_port_label_image_{-1};
  int output_port_pose_{-1};
};

}  // namespace dev
}  // namespace sensors
}  // namespace systems
}  // namespace drake