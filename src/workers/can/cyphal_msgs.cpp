//CYPHAL_HEADERS_BEGIN
#include <reg/udral/physics/acoustics/Note_0_1.h>
#include <reg/udral/physics/kinematics/geodetic/PointState_0_1.h>
#include <reg/udral/physics/kinematics/geodetic/PointStateVarTs_0_1.h>
#include <reg/udral/physics/kinematics/geodetic/StateVar_0_1.h>
#include <reg/udral/physics/kinematics/geodetic/State_0_1.h>
#include <reg/udral/physics/kinematics/geodetic/StateVarTs_0_1.h>
#include <reg/udral/physics/kinematics/geodetic/PointStateVar_0_1.h>
#include <reg/udral/physics/kinematics/geodetic/Point_0_1.h>
#include <reg/udral/physics/kinematics/geodetic/PoseVar_0_1.h>
#include <reg/udral/physics/kinematics/geodetic/PointVar_0_1.h>
#include <reg/udral/physics/kinematics/geodetic/Pose_0_1.h>
#include <reg/udral/physics/kinematics/translation/Linear_0_1.h>
#include <reg/udral/physics/kinematics/translation/Velocity1VarTs_0_1.h>
#include <reg/udral/physics/kinematics/translation/Velocity3Var_0_2.h>
#include <reg/udral/physics/kinematics/translation/Velocity3Var_0_1.h>
#include <reg/udral/physics/kinematics/translation/LinearVarTs_0_1.h>
#include <reg/udral/physics/kinematics/translation/LinearTs_0_1.h>
#include <reg/udral/physics/kinematics/rotation/PlanarTs_0_1.h>
#include <reg/udral/physics/kinematics/rotation/Planar_0_1.h>
#include <reg/udral/physics/kinematics/cartesian/PointState_0_1.h>
#include <reg/udral/physics/kinematics/cartesian/PointStateVarTs_0_1.h>
#include <reg/udral/physics/kinematics/cartesian/StateVar_0_1.h>
#include <reg/udral/physics/kinematics/cartesian/PoseVarTs_0_1.h>
#include <reg/udral/physics/kinematics/cartesian/State_0_1.h>
#include <reg/udral/physics/kinematics/cartesian/StateVarTs_0_1.h>
#include <reg/udral/physics/kinematics/cartesian/PointStateVar_0_1.h>
#include <reg/udral/physics/kinematics/cartesian/Point_0_1.h>
#include <reg/udral/physics/kinematics/cartesian/PoseVar_0_1.h>
#include <reg/udral/physics/kinematics/cartesian/PointVar_0_1.h>
#include <reg/udral/physics/kinematics/cartesian/TwistVarTs_0_1.h>
#include <reg/udral/physics/kinematics/cartesian/TwistVar_0_1.h>
#include <reg/udral/physics/kinematics/cartesian/Pose_0_1.h>
#include <reg/udral/physics/kinematics/cartesian/Twist_0_1.h>
#include <reg/udral/physics/electricity/Source_0_1.h>
#include <reg/udral/physics/electricity/Power_0_1.h>
#include <reg/udral/physics/electricity/SourceTs_0_1.h>
#include <reg/udral/physics/electricity/PowerTs_0_1.h>
#include <reg/udral/physics/optics/HighColor_0_1.h>
#include <reg/udral/physics/acoustics/Note_0_1.h>
#include <reg/udral/physics/dynamics/translation/Linear_0_1.h>
#include <reg/udral/physics/dynamics/translation/LinearTs_0_1.h>
#include <reg/udral/physics/dynamics/rotation/PlanarTs_0_1.h>
#include <reg/udral/physics/dynamics/rotation/Planar_0_1.h>
#include <reg/udral/physics/time/TAI64VarTs_0_1.h>
#include <reg/udral/physics/time/TAI64Var_0_1.h>
#include <reg/udral/physics/time/TAI64_0_1.h>
#include <reg/udral/physics/thermodynamics/PressureTempVarTs_0_1.h>
#include <reg/udral/service/actuator/common/sp/Vector8_0_1.h>
#include <reg/udral/service/actuator/common/sp/Vector4_0_1.h>
#include <reg/udral/service/actuator/common/sp/Vector2_0_1.h>
#include <reg/udral/service/actuator/common/sp/_0_1.h>
#include <reg/udral/service/actuator/common/sp/Vector31_0_1.h>
#include <reg/udral/service/actuator/common/sp/Scalar_0_1.h>
#include <reg/udral/service/actuator/common/sp/Vector3_0_1.h>
#include <reg/udral/service/actuator/common/sp/Vector6_0_1.h>
#include <reg/udral/service/actuator/common/Status_0_1.h>
#include <reg/udral/service/actuator/common/Feedback_0_1.h>
#include <reg/udral/service/actuator/common/_0_1.h>
#include <reg/udral/service/actuator/common/FaultFlags_0_1.h>
#include <reg/udral/service/actuator/esc/_0_1.h>
#include <reg/udral/service/actuator/servo/_0_1.h>
#include <reg/udral/service/common/Readiness_0_1.h>
#include <reg/udral/service/common/Heartbeat_0_1.h>
#include <reg/udral/service/battery/Status_0_2.h>
#include <reg/udral/service/battery/Error_0_1.h>
#include <reg/udral/service/battery/Parameters_0_3.h>
#include <reg/udral/service/battery/_0_1.h>
#include <reg/udral/service/battery/Technology_0_1.h>
#include <reg/udral/service/sensor/Status_0_1.h>
#include <uavcan/_register/Value_1_0.h>
#include <uavcan/_register/Access_1_0.h>
#include <uavcan/_register/Name_1_0.h>
#include <uavcan/_register/List_1_0.h>
#include <uavcan/si/sample/duration/WideScalar_1_0.h>
#include <uavcan/si/sample/duration/Scalar_1_0.h>
#include <uavcan/si/sample/acceleration/Vector3_1_0.h>
#include <uavcan/si/sample/acceleration/Scalar_1_0.h>
#include <uavcan/si/sample/force/Vector3_1_0.h>
#include <uavcan/si/sample/force/Scalar_1_0.h>
#include <uavcan/si/sample/angular_acceleration/Vector3_1_0.h>
#include <uavcan/si/sample/angular_acceleration/Scalar_1_0.h>
#include <uavcan/si/sample/magnetic_field_strength/Scalar_1_0.h>
#include <uavcan/si/sample/magnetic_field_strength/Vector3_1_0.h>
#include <uavcan/si/sample/magnetic_field_strength/Vector3_1_1.h>
#include <uavcan/si/sample/magnetic_field_strength/Scalar_1_0.h>
#include <uavcan/si/sample/magnetic_field_strength/Scalar_1_1.h>
#include <uavcan/si/sample/electric_current/Scalar_1_0.h>
#include <uavcan/si/sample/volume/Scalar_1_0.h>
#include <uavcan/si/sample/mass/Scalar_1_0.h>
#include <uavcan/si/sample/temperature/Scalar_1_0.h>
#include <uavcan/si/sample/pressure/Scalar_1_0.h>
#include <uavcan/si/sample/frequency/Scalar_1_0.h>
#include <uavcan/si/sample/energy/Scalar_1_0.h>
#include <uavcan/si/sample/angle/Scalar_1_0.h>
#include <uavcan/si/sample/angle/Quaternion_1_0.h>
#include <uavcan/si/sample/torque/Vector3_1_0.h>
#include <uavcan/si/sample/torque/Scalar_1_0.h>
#include <uavcan/si/sample/electric_charge/Scalar_1_0.h>
#include <uavcan/si/sample/magnetic_flux_density/Vector3_1_0.h>
#include <uavcan/si/sample/magnetic_flux_density/Scalar_1_0.h>
#include <uavcan/si/sample/angular_velocity/Vector3_1_0.h>
#include <uavcan/si/sample/angular_velocity/Scalar_1_0.h>
#include <uavcan/si/sample/power/Scalar_1_0.h>
#include <uavcan/si/sample/length/Vector3_1_0.h>
#include <uavcan/si/sample/length/WideScalar_1_0.h>
#include <uavcan/si/sample/length/WideVector3_1_0.h>
#include <uavcan/si/sample/length/Scalar_1_0.h>
#include <uavcan/si/sample/volumetric_flow_rate/Scalar_1_0.h>
#include <uavcan/si/sample/voltage/Scalar_1_0.h>
#include <uavcan/si/sample/luminance/Scalar_1_0.h>
#include <uavcan/si/sample/velocity/Vector3_1_0.h>
#include <uavcan/si/sample/velocity/Scalar_1_0.h>
#include <uavcan/si/unit/duration/WideScalar_1_0.h>
#include <uavcan/si/unit/duration/Scalar_1_0.h>
#include <uavcan/si/unit/acceleration/Vector3_1_0.h>
#include <uavcan/si/unit/acceleration/Scalar_1_0.h>
#include <uavcan/si/unit/force/Vector3_1_0.h>
#include <uavcan/si/unit/force/Scalar_1_0.h>
#include <uavcan/si/unit/angular_acceleration/Vector3_1_0.h>
#include <uavcan/si/unit/angular_acceleration/Scalar_1_0.h>
#include <uavcan/si/unit/magnetic_field_strength/Vector3_1_0.h>
#include <uavcan/si/unit/magnetic_field_strength/Vector3_1_1.h>
#include <uavcan/si/unit/magnetic_field_strength/Scalar_1_0.h>
#include <uavcan/si/unit/magnetic_field_strength/Scalar_1_1.h>
#include <uavcan/si/unit/electric_current/Scalar_1_0.h>
#include <uavcan/si/unit/volume/Scalar_1_0.h>
#include <uavcan/si/unit/mass/Scalar_1_0.h>
#include <uavcan/si/unit/temperature/Scalar_1_0.h>
#include <uavcan/si/unit/pressure/Scalar_1_0.h>
#include <uavcan/si/unit/frequency/Scalar_1_0.h>
#include <uavcan/si/unit/energy/Scalar_1_0.h>
#include <uavcan/si/unit/angle/Scalar_1_0.h>
#include <uavcan/si/unit/angle/Quaternion_1_0.h>
#include <uavcan/si/unit/torque/Vector3_1_0.h>
#include <uavcan/si/unit/torque/Scalar_1_0.h>
#include <uavcan/si/unit/electric_charge/Scalar_1_0.h>
#include <uavcan/si/unit/magnetic_flux_density/Vector3_1_0.h>
#include <uavcan/si/unit/magnetic_flux_density/Scalar_1_0.h>
#include <uavcan/si/unit/angular_velocity/Vector3_1_0.h>
#include <uavcan/si/unit/angular_velocity/Scalar_1_0.h>
#include <uavcan/si/unit/power/Scalar_1_0.h>
#include <uavcan/si/unit/length/Vector3_1_0.h>
#include <uavcan/si/unit/length/WideScalar_1_0.h>
#include <uavcan/si/unit/length/WideVector3_1_0.h>
#include <uavcan/si/unit/length/Scalar_1_0.h>
#include <uavcan/si/unit/volumetric_flow_rate/Scalar_1_0.h>
#include <uavcan/si/unit/voltage/Scalar_1_0.h>
#include <uavcan/si/unit/luminance/Scalar_1_0.h>
#include <uavcan/si/unit/velocity/Vector3_1_0.h>
#include <uavcan/si/unit/velocity/Scalar_1_0.h>
#include <uavcan/file/List_0_1.h>
#include <uavcan/file/Path_1_0.h>
#include <uavcan/file/List_0_2.h>
#include <uavcan/file/Error_1_0.h>
#include <uavcan/file/GetInfo_0_2.h>
#include <uavcan/file/Modify_1_0.h>
#include <uavcan/file/Modify_1_1.h>
#include <uavcan/file/Path_2_0.h>
#include <uavcan/file/Write_1_1.h>
#include <uavcan/file/Read_1_0.h>
#include <uavcan/file/Write_1_0.h>
#include <uavcan/file/GetInfo_0_1.h>
#include <uavcan/file/Read_1_1.h>
#include <uavcan/node/IOStatistics_0_1.h>
#include <uavcan/node/Mode_1_0.h>
#include <uavcan/node/ExecuteCommand_1_0.h>
#include <uavcan/node/Heartbeat_1_0.h>
#include <uavcan/node/GetInfo_1_0.h>
#include <uavcan/node/GetTransportStatistics_0_1.h>
#include <uavcan/node/ExecuteCommand_1_2.h>
#include <uavcan/node/ID_1_0.h>
#include <uavcan/node/port/SubjectIDList_0_1.h>
#include <uavcan/node/port/List_0_1.h>
#include <uavcan/node/port/SubjectIDList_1_0.h>
#include <uavcan/node/port/ID_1_0.h>
#include <uavcan/node/port/ServiceID_1_0.h>
#include <uavcan/node/port/SubjectID_1_0.h>
#include <uavcan/node/port/List_1_0.h>
#include <uavcan/node/Version_1_0.h>
#include <uavcan/node/ExecuteCommand_1_1.h>
#include <uavcan/node/Health_1_0.h>
#include <uavcan/time/Synchronization_1_0.h>
#include <uavcan/time/GetSynchronizationMasterInfo_0_1.h>
#include <uavcan/time/TAIInfo_0_1.h>
#include <uavcan/time/SynchronizedTimestamp_1_0.h>
#include <uavcan/time/TimeSystem_0_1.h>
#include <uavcan/primitive/scalar/Real64_1_0.h>
#include <uavcan/primitive/scalar/Natural64_1_0.h>
#include <uavcan/primitive/scalar/Integer8_1_0.h>
#include <uavcan/primitive/scalar/Integer32_1_0.h>
#include <uavcan/primitive/scalar/Bit_1_0.h>
#include <uavcan/primitive/scalar/Integer16_1_0.h>
#include <uavcan/primitive/scalar/Integer64_1_0.h>
#include <uavcan/primitive/scalar/Real16_1_0.h>
#include <uavcan/primitive/scalar/Natural8_1_0.h>
#include <uavcan/primitive/scalar/Natural32_1_0.h>
#include <uavcan/primitive/scalar/Real32_1_0.h>
#include <uavcan/primitive/scalar/Natural16_1_0.h>
#include <uavcan/primitive/Empty_1_0.h>
#include <uavcan/primitive/array/Real64_1_0.h>
#include <uavcan/primitive/array/Natural64_1_0.h>
#include <uavcan/primitive/array/Integer8_1_0.h>
#include <uavcan/primitive/array/Integer32_1_0.h>
#include <uavcan/primitive/array/Bit_1_0.h>
#include <uavcan/primitive/array/Integer16_1_0.h>
#include <uavcan/primitive/array/Integer64_1_0.h>
#include <uavcan/primitive/array/Real16_1_0.h>
#include <uavcan/primitive/array/Natural8_1_0.h>
#include <uavcan/primitive/array/Natural32_1_0.h>
#include <uavcan/primitive/array/Real32_1_0.h>
#include <uavcan/primitive/array/Natural16_1_0.h>
#include <uavcan/primitive/String_1_0.h>
#include <uavcan/primitive/Unstructured_1_0.h>
//CYPHAL_HEADERS_END


#include "cyphal_helpers.h"

#include <QVariant>
#include "describe/describe.hpp"
#include "radapter/config.hpp"
#include "radapter/logs.hpp"

template<typename T>
using SerializeImpl = int8_t(*)(const T* const obj, uint8_t* const buffer, size_t* const inout_buffer_size_bytes);

template<typename T>
using DeserializeImpl = int8_t(*)(T* const obj, const uint8_t* buffer, size_t* const inout_buffer_size_bytes);

struct FieldIsFixedString {};
struct FieldIsDynString {};
struct FieldIsDynArray {};

template<typename T, SerializeImpl<T> impl>
static void serialize(QVariant const& data, uint8_t* buffer, size_t size)
{
    T value;
    radapter::Parse(value, data);
    if (impl(&value, buffer, &size))
        radapter::Raise("Cyphal: error serializing type: {}", describe::Get<T>::name);
}

template<typename T, DeserializeImpl<T> impl>
static QVariant deserialize(const uint8_t* buffer, size_t size)
{
    T value;
    if (impl(&value, buffer, &size))
        radapter::Raise("Cyphal: error serializing type: {}", describe::Get<T>::name);
    // TODO: use describe here to convert TO QVariant
}

template<int I>
struct DynType {
    static constexpr const radapter::can::CanardMessageDynamic* value = nullptr;
};

auto collect_types(...) -> void;

constexpr int _types_begin = __COUNTER__;

#define CYPHAL_TYPE(name, ver, ...) \
    DESCRIBE(#name, name##_##ver, void) \
    { \
        __VA_ARGS__ \
    } \
    constexpr radapter::can::CanardMessageDynamic Dyn_##name##_##ver = { \
        u"" #name, \
        u"" name##_##ver##_FULL_NAME_, \
        u"" name##_##ver##_FULL_NAME_AND_VERSION_, \
        name##_##ver##_EXTENT_BYTES_, \
        deserialize<name##_##ver, name##_##ver##_deserialize_>, \
        serialize<name##_##ver, name##_##ver##_serialize_>, \
    }; \
    template<>\
    struct DynType<__COUNTER__ - _types_begin> { static constexpr const radapter::can::CanardMessageDynamic* value = &Dyn_##name##_##ver; };

//CYPHAL_TYPES_BEGIN
CYPHAL_TYPE(uavcan_si_unit_duration_WideScalar, 1_0,
    RAD_MEMBER(second);)
CYPHAL_TYPE(uavcan_si_unit_duration_Scalar, 1_0,
    RAD_MEMBER(second);)
CYPHAL_TYPE(uavcan_si_unit_acceleration_Vector3, 1_0,
    RAD_MEMBER(meter_per_second_per_second);)
CYPHAL_TYPE(uavcan_si_unit_acceleration_Scalar, 1_0,
    RAD_MEMBER(meter_per_second_per_second);)
CYPHAL_TYPE(uavcan_si_unit_force_Vector3, 1_0,
    RAD_MEMBER(newton);)
CYPHAL_TYPE(uavcan_si_unit_force_Scalar, 1_0,
    RAD_MEMBER(newton);)
CYPHAL_TYPE(uavcan_si_unit_angular_acceleration_Vector3, 1_0,
    RAD_MEMBER(radian_per_second_per_second);)
CYPHAL_TYPE(uavcan_si_unit_angular_acceleration_Scalar, 1_0,
    RAD_MEMBER(radian_per_second_per_second);)
CYPHAL_TYPE(uavcan_si_unit_magnetic_field_strength_Vector3, 1_0,
    RAD_MEMBER(tesla);)
CYPHAL_TYPE(uavcan_si_unit_magnetic_field_strength_Vector3, 1_1,
    RAD_MEMBER(ampere_per_meter);)
CYPHAL_TYPE(uavcan_si_unit_magnetic_field_strength_Scalar, 1_0,
    RAD_MEMBER(tesla);)
CYPHAL_TYPE(uavcan_si_unit_magnetic_field_strength_Scalar, 1_1,
    RAD_MEMBER(ampere_per_meter);)
CYPHAL_TYPE(uavcan_si_unit_electric_current_Scalar, 1_0,
    RAD_MEMBER(ampere);)
CYPHAL_TYPE(uavcan_si_unit_volume_Scalar, 1_0,
    RAD_MEMBER(cubic_meter);)
CYPHAL_TYPE(uavcan_si_unit_mass_Scalar, 1_0,
    RAD_MEMBER(kilogram);)
CYPHAL_TYPE(uavcan_si_unit_temperature_Scalar, 1_0,
    RAD_MEMBER(kelvin);)
CYPHAL_TYPE(uavcan_si_unit_pressure_Scalar, 1_0,
    RAD_MEMBER(pascal);)
CYPHAL_TYPE(uavcan_si_unit_frequency_Scalar, 1_0,
    RAD_MEMBER(hertz);)
CYPHAL_TYPE(uavcan_si_unit_energy_Scalar, 1_0,
    RAD_MEMBER(joule);)
CYPHAL_TYPE(uavcan_si_unit_angle_Scalar, 1_0,
    RAD_MEMBER(radian);)
CYPHAL_TYPE(uavcan_si_unit_angle_Quaternion, 1_0,
    RAD_MEMBER(wxyz);)
CYPHAL_TYPE(uavcan_si_unit_torque_Vector3, 1_0,
    RAD_MEMBER(newton_meter);)
CYPHAL_TYPE(uavcan_si_unit_torque_Scalar, 1_0,
    RAD_MEMBER(newton_meter);)
CYPHAL_TYPE(uavcan_si_unit_electric_charge_Scalar, 1_0,
    RAD_MEMBER(coulomb);)
CYPHAL_TYPE(uavcan_si_unit_magnetic_flux_density_Vector3, 1_0,
    RAD_MEMBER(tesla);)
CYPHAL_TYPE(uavcan_si_unit_magnetic_flux_density_Scalar, 1_0,
    RAD_MEMBER(tesla);)
CYPHAL_TYPE(uavcan_si_unit_angular_velocity_Vector3, 1_0,
    RAD_MEMBER(radian_per_second);)
CYPHAL_TYPE(uavcan_si_unit_angular_velocity_Scalar, 1_0,
    RAD_MEMBER(radian_per_second);)
CYPHAL_TYPE(uavcan_si_unit_power_Scalar, 1_0,
    RAD_MEMBER(watt);)
CYPHAL_TYPE(uavcan_si_unit_length_Vector3, 1_0,
    RAD_MEMBER(meter);)
CYPHAL_TYPE(uavcan_si_unit_length_WideScalar, 1_0,
    RAD_MEMBER(meter);)
CYPHAL_TYPE(uavcan_si_unit_length_WideVector3, 1_0,
    RAD_MEMBER(meter);)
CYPHAL_TYPE(uavcan_si_unit_length_Scalar, 1_0,
    RAD_MEMBER(meter);)
CYPHAL_TYPE(uavcan_si_unit_volumetric_flow_rate_Scalar, 1_0,
    RAD_MEMBER(cubic_meter_per_second);)
CYPHAL_TYPE(uavcan_si_unit_voltage_Scalar, 1_0,
    RAD_MEMBER(volt);)
CYPHAL_TYPE(uavcan_si_unit_luminance_Scalar, 1_0,
    RAD_MEMBER(candela_per_square_meter);)
CYPHAL_TYPE(uavcan_si_unit_velocity_Vector3, 1_0,
    RAD_MEMBER(meter_per_second);)
CYPHAL_TYPE(uavcan_si_unit_velocity_Scalar, 1_0,
    RAD_MEMBER(meter_per_second);)
CYPHAL_TYPE(uavcan_node_IOStatistics, 0_1,
    RAD_MEMBER(num_emitted);
    RAD_MEMBER(num_errored);
    RAD_MEMBER(num_received);)
CYPHAL_TYPE(uavcan_node_Mode, 1_0,
    RAD_MEMBER(value);)

CYPHAL_TYPE(uavcan_node_Heartbeat, 1_0,
    RAD_MEMBER(health);
    RAD_MEMBER(mode);
    RAD_MEMBER(uptime);
    RAD_MEMBER(vendor_specific_status_code);)

CYPHAL_TYPE(uavcan_node_ExecuteCommand_Request, 1_0,
    RAD_MEMBER(command);
    MEMBER("parameter", &_::parameter, FieldIsDynString);)
CYPHAL_TYPE(uavcan_node_ExecuteCommand_Response, 1_0,
    RAD_MEMBER(status);)
CYPHAL_TYPE(uavcan_node_GetInfo_Request, 1_0,)
CYPHAL_TYPE(uavcan_node_GetInfo_Response, 1_0,
    RAD_MEMBER(protocol_version);
    RAD_MEMBER(hardware_version);
    RAD_MEMBER(software_version);
    RAD_MEMBER(software_vcs_revision_id);
    MEMBER("unique_id", &_::unique_id, FieldIsFixedString);
    MEMBER("name", &_::name, FieldIsDynString);
    MEMBER("software_image_crc", &_::software_image_crc, FieldIsDynString);
    MEMBER("certificate_of_authenticity", &_::certificate_of_authenticity, FieldIsDynString);)
CYPHAL_TYPE(uavcan_node_GetTransportStatistics_Request, 0_1,)
CYPHAL_TYPE(uavcan_node_GetTransportStatistics_Response, 0_1,
    RAD_MEMBER(transfer_statistics);
    MEMBER("network_interface_statistics", &_::network_interface_statistics, FieldIsDynArray);)

CYPHAL_TYPE(uavcan_node_ExecuteCommand_Request, 1_1,
    RAD_MEMBER(command);
    MEMBER("parameter", &_::parameter, FieldIsDynString);)
CYPHAL_TYPE(uavcan_node_ExecuteCommand_Response, 1_1,
    RAD_MEMBER(status);)

CYPHAL_TYPE(uavcan_node_ExecuteCommand_Request, 1_2,
    RAD_MEMBER(command);
    MEMBER("parameter", &_::parameter, FieldIsDynString);)
CYPHAL_TYPE(uavcan_node_ExecuteCommand_Response, 1_2,
    RAD_MEMBER(status);)

CYPHAL_TYPE(uavcan_node_ID, 1_0,
    RAD_MEMBER(value);)
CYPHAL_TYPE(uavcan_node_port_SubjectIDList, 1_0,)
CYPHAL_TYPE(uavcan_node_port_ID, 1_0,)
CYPHAL_TYPE(uavcan_node_port_ServiceID, 1_0,)
CYPHAL_TYPE(uavcan_node_port_SubjectID, 1_0,)
CYPHAL_TYPE(uavcan_node_port_List, 1_0,
    RAD_MEMBER(clients);
    RAD_MEMBER(publishers);
    RAD_MEMBER(servers);
    RAD_MEMBER(subscribers);)
CYPHAL_TYPE(uavcan_node_Version, 1_0,)


CYPHAL_TYPE(uavcan_node_Health, 1_0,)
CYPHAL_TYPE(uavcan_time_Synchronization, 1_0,)

CYPHAL_TYPE(uavcan_time_GetSynchronizationMasterInfo_Request, 0_1,)
CYPHAL_TYPE(uavcan_time_GetSynchronizationMasterInfo_Response, 0_1,)

CYPHAL_TYPE(uavcan_time_TAIInfo, 0_1,)
CYPHAL_TYPE(uavcan_time_SynchronizedTimestamp, 1_0,)
CYPHAL_TYPE(uavcan_time_TimeSystem, 0_1,)

CYPHAL_TYPE(uavcan_primitive_scalar_Real64, 1_0,)
CYPHAL_TYPE(uavcan_primitive_scalar_Natural64, 1_0,)
CYPHAL_TYPE(uavcan_primitive_scalar_Integer8, 1_0,)
CYPHAL_TYPE(uavcan_primitive_scalar_Integer32, 1_0,)
CYPHAL_TYPE(uavcan_primitive_scalar_Bit, 1_0,)
CYPHAL_TYPE(uavcan_primitive_scalar_Integer16, 1_0,)
CYPHAL_TYPE(uavcan_primitive_scalar_Integer64, 1_0,)
CYPHAL_TYPE(uavcan_primitive_scalar_Real16, 1_0,)
CYPHAL_TYPE(uavcan_primitive_scalar_Natural8, 1_0,)
CYPHAL_TYPE(uavcan_primitive_scalar_Natural32, 1_0,)
CYPHAL_TYPE(uavcan_primitive_scalar_Real32, 1_0,)
CYPHAL_TYPE(uavcan_primitive_scalar_Natural16, 1_0,)
CYPHAL_TYPE(uavcan_primitive_Empty, 1_0,)

CYPHAL_TYPE(uavcan_primitive_array_Real64, 1_0,)
CYPHAL_TYPE(uavcan_primitive_array_Natural64, 1_0,)
CYPHAL_TYPE(uavcan_primitive_array_Integer8, 1_0,)
CYPHAL_TYPE(uavcan_primitive_array_Integer32, 1_0,)
CYPHAL_TYPE(uavcan_primitive_array_Bit, 1_0,)
CYPHAL_TYPE(uavcan_primitive_array_Integer16, 1_0,)
CYPHAL_TYPE(uavcan_primitive_array_Integer64, 1_0,)
CYPHAL_TYPE(uavcan_primitive_array_Real16, 1_0,)
CYPHAL_TYPE(uavcan_primitive_array_Natural8, 1_0,)
CYPHAL_TYPE(uavcan_primitive_array_Natural32, 1_0,)
CYPHAL_TYPE(uavcan_primitive_array_Real32, 1_0,)
CYPHAL_TYPE(uavcan_primitive_array_Natural16, 1_0,)
CYPHAL_TYPE(uavcan_primitive_String, 1_0,)
CYPHAL_TYPE(uavcan_primitive_Unstructured, 1_0,)

CYPHAL_TYPE(uavcan_register_Value, 1_0,)
CYPHAL_TYPE(uavcan_register_Access_Request, 1_0,)
CYPHAL_TYPE(uavcan_register_Access_Response, 1_0,)
CYPHAL_TYPE(uavcan_register_Name, 1_0,)
CYPHAL_TYPE(uavcan_register_List_Request, 1_0,)
CYPHAL_TYPE(uavcan_register_List_Response, 1_0,)
//CYPHAL_TYPES_END

constexpr int types_end = __COUNTER__ - _types_begin - 1;

namespace radapter::can
{

QMap<QStringView, const CanardMessageDynamic*> by_name;
QMap<QStringView, const CanardMessageDynamic*> by_full_name;
QMap<QStringView, const CanardMessageDynamic*> by_full_name_and_ver;

template<size_t...Is>
static bool do_init_msgs(std::index_sequence<Is...>)
{
    by_name = {{DynType<Is+1>::value->name, DynType<Is+1>::value}...};
    by_full_name = {{DynType<Is+1>::value->full_name, DynType<Is+1>::value}...};
    by_full_name_and_ver = {{DynType<Is+1>::value->full_name_and_ver, DynType<Is+1>::value}...};
    return true;
}

const CanardMessageDynamic* lookup_canard_type(QStringView name)
{
    static [[maybe_unused]] bool _ = do_init_msgs(std::make_index_sequence<types_end>{});
    const CanardMessageDynamic* res = nullptr;
    (void)((res = by_name.find(name)) || (res = by_full_name.find(name)) || (res = by_full_name_and_ver.find(name)));
    return res;
}

}