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


using namespace radapter;


template<typename T>
using SerializeImpl = int8_t(*)(const T* const obj, uint8_t* const buffer, size_t* const inout_buffer_size_bytes);

template<typename T>
using DeserializeImpl = int8_t(*)(T* const obj, const uint8_t* buffer, size_t* const inout_buffer_size_bytes);


struct TypeIsUnionBase {}; //will have _tag_ member which selects one of the fields

template<typename Enum> //use Enum for string <-> int translation of tag
struct TypeIsUnion
{
    using type = Enum;
};

struct FieldIsBitArray {}; // Access via @ref nunavutSetBit(), @ref nunavutGetBit().
struct FieldIsBitmask {}; // Access via @ref nunavutSetBit(), @ref nunavutGetBit().
struct FieldIsUniqueID {};
struct FieldIsDynString {};
struct FieldIsDynArray {}; // elements + count
struct FieldIsEnumBase {};

template <typename E>
struct FieldIsEnum : FieldIsEnumBase {
    using type = E;
};

template<typename T> void to_variant(T const& in, QVariant& out, TraceFrame const& frame);
template<typename T> void from_variant(QVariant const&, T& out, const TraceFrame& frame);

template<typename T, typename Member>
void field_to_variant(T const& in, Member info, QVariant& out, [[maybe_unused]] TraceFrame const& frame)
{
    auto& field = info.get(in);
    if constexpr (describe::has_v<FieldIsBitArray, Member>) {
        size_t count = field.count;
        auto packed = field.bitpacked;
        QVariantList res;
        res.reserve(int(count));
        for(size_t i = 0; i < count; ++i) {
            bool bit = nunavutGetBit(packed, sizeof(packed), i);
            res.push_back(bool(bit));
        }
        out = std::move(res);
    } else if constexpr (describe::has_v<FieldIsUniqueID, Member>) {
        std::string res;
        for (auto byte: field) {
            res += fmt::format("{:x}", byte);
        }
        out = QString::fromLatin1(res.c_str(), int(res.size()));
    } else if constexpr (describe::has_v<FieldIsBitmask, Member>) {
        QVariantList res;
        for (size_t i = 0; i < sizeof(field) * 8; ++i) {
            if (nunavutGetBit(field, sizeof(field), i)) {
                res.append(unsigned(i));
            }
        }
        out = std::move(res);
    } else if constexpr (describe::has_v<FieldIsDynString, Member>) {
        size_t count = field.count;
        auto* chars = reinterpret_cast<const char*>(field.elements);
        out = QString::fromUtf8(chars, int(count));
    } else if constexpr (describe::has_v<FieldIsDynArray, Member>) {
        size_t count = field.count;
        auto& elems = field.elements;
        QVariantList res;
        res.reserve(int(count));
        for (size_t i = 0; i < count; ++i) {
            QVariant item;
            to_variant(elems[i], item, TraceFrame(uint32_t(i), frame));
            res.push_back(item);
        }
        out = std::move(res);
    } else if constexpr (describe::has_v<FieldIsEnumBase, Member>) {
        using E = typename describe::extract_t<FieldIsEnumBase, Member>::type;
        std::string_view name;
        if (!describe::enum_to_name(E(field), name)) {
            radapter::Raise("Invalid value for enum({}) -> 0x{:x}", describe::Get<E>::name, field, frame);
        }
        out = QString::fromLatin1(name.data(), int(name.size()));
    } else if constexpr (std::rank_v<typename Member::type>) {
        size_t count = std::size(field);
        QVariantList res;
        res.reserve(int(count));
        for (size_t i = 0; i < count; ++i) {
            QVariant item;
            to_variant(field[i], item, TraceFrame(uint32_t(i), frame));
            res.push_back(item);
        }
        out = std::move(res);
    } else {
        to_variant(field, out, frame);
    }
}

template<typename T>
void to_variant(T const& in, QVariant& out, [[maybe_unused]] TraceFrame const& frame)
{
    if constexpr (!describe::is_described_v<T>)
    {
        out = QVariant::fromValue(in);
    }
    else if constexpr (describe::has_v<TypeIsUnionBase, T>)
    {
        using UnionEnum = typename describe::extract_t<TypeIsUnionBase, T>::type;
        UnionEnum tag = UnionEnum(in._tag_);
        int index = 0;
        bool ok = false;
        QVariantMap result;
        describe::Get<T>::for_each([&](auto info){
            if (UnionEnum(index++) == tag)
            {
                ok = true;
                field_to_variant(in, info, result[QString::fromLatin1(info.name.data(), int(info.name.size()))], TraceFrame(info.name, frame));
            }
        });
        if (!ok) {
            radapter::Raise("{}: Invalid (or incorrectly described) _tag_ field in {} -> 0x{:x}", frame, describe::Get<T>::name, fmt::underlying(tag));
        }
        out = std::move(result);
    }
    else
    {
        QVariantMap result;
        describe::Get<T>::for_each([&](auto info){
            field_to_variant(in, info, result[QString::fromLatin1(info.name.data(), int(info.name.size()))], TraceFrame(info.name, frame));
        });
        out = result;
    }
}


template<typename T, typename Member>
void field_from_variant(const QVariantMap& in, Member info, T& out, [[maybe_unused]] TraceFrame const& frame)
{
    auto& field = info.get(out);
    auto it = in.find(QString::fromLatin1(info.name.data(), int(info.name.size())));
    if (it == in.end())
        Raise("{}: type: {}: not found", frame, describe::Get<T>::name);
    const QVariant& val = it.value();
    if constexpr (describe::has_v<FieldIsBitArray, Member>) {
        size_t max_sz = std::size(field.bitpacked);
        if (val.type() != QVariant::List) {
            Raise("{}: {}: Expected a list, got: {}", frame, describe::Get<T>::name, TypeNameOf(val));
        }
        const auto list = val.toList();
        if (size_t(list.size()) > max_sz)
            Raise("{}: {}: List exceeds max size of {}, got: {}", frame, describe::Get<T>::name, max_sz, list.size());
        uint32_t idx = 0;
        for(auto& v: list) {
            auto i = idx++;
            if (v.toBool()) {
                nunavutSetBit(field.bitpacked, sizeof(field.bitpacked), i, true);
            } else {
                nunavutSetBit(field.bitpacked, sizeof(field.bitpacked), i, false);
            }
        }
        field.count = size_t(list.size());
    } else if constexpr (describe::has_v<FieldIsUniqueID, Member>) {
        if (val.type() != QVariant::String) {
            Raise("{}: {}: Expected a unique_id (string), got: {}", frame, describe::Get<T>::name, TypeNameOf(val));
        }
        ::memset(field, 0, sizeof(field));
        const auto str = val.toString();
        if (size_t(str.size()) > sizeof(field))
            Raise("{}: {}: Expected a unique_id (string), string too long: {}, max: {}", frame, describe::Get<T>::name, str.size(), sizeof(field));
        for (int i = 0; i < str.size(); ++i) {
            field[i] = static_cast<uint8_t>(str[i].digitValue());
        }
    } else if constexpr (describe::has_v<FieldIsBitmask, Member>) {
        if (val.type() != QVariant::List) {
            Raise("{}: {}: Expected a list of bits, got: {}", frame, describe::Get<T>::name, TypeNameOf(val));
        }
        const auto list = val.toList();
        size_t max_bit = sizeof(field) * 8;
        size_t idx = 0;
        ::memset(field, 0, sizeof(field));
        for (auto& v: list) {
            auto i = idx++;
            bool ok;
            auto u = v.toUInt(&ok);
            if (!ok || u > max_bit)
                Raise("{}: {}: could not cast item#{} (type: {}) to bit index (max index: {})", frame, describe::Get<T>::name, i, TypeNameOf(v), max_bit);
            nunavutSetBit(field, sizeof(field), u, true);
        }
    } else if constexpr (describe::has_v<FieldIsDynString, Member>) {
        auto max_sz = std::size(field.elements) - 1;
        if (val.type() != QVariant::String) {
            Raise("{}: {}: Expected a string, got: {}", frame, describe::Get<T>::name, TypeNameOf(val));
        }
        const auto str = val.toString().toStdString();
        if (size_t(str.size()) > max_sz)
            Raise("{}: {}: List exceeds max size of {}, got: {}", frame, describe::Get<T>::name, max_sz, str.size());

        ::memcpy(field.elements, str.c_str(), str.size() + 1);
        field.count = str.size();
    } else if constexpr (describe::has_v<FieldIsDynArray, Member>) {
        auto max_sz = std::size(field.elements);
        if (val.type() != QVariant::List && val.type() != QVariant::StringList) {
            Raise("{}: {}: Expected a list, got: {}", frame, describe::Get<T>::name, TypeNameOf(val));
        }
        const auto list = val.toList();
        if (size_t(list.size()) > max_sz)
            Raise("{}: {}: List exceeds max size of {}, got: {}", frame, describe::Get<T>::name, max_sz, list.size());
        uint32_t idx = 0;
        for(auto& v: list) {
            auto i = idx++;
            from_variant(v, field.elements[i], TraceFrame(i, frame));
        }
        field.count = size_t(list.size());
    } else if constexpr (describe::has_v<FieldIsEnumBase, Member>) {
        using E = typename describe::extract_t<FieldIsEnumBase, Member>::type;
        const auto str = val.toString();
        if (str.isEmpty())
            Raise("{}: {}: expected string, got: {}", frame, describe::Get<T>::name, TypeNameOf(val));
        const auto l1 = str.toLatin1();
        E res;
        if (!describe::name_to_enum({l1.constData(), size_t(l1.size())}, res))
            Raise("{}: {}: invalid enum value: {}. Valid: [{}]", frame, describe::Get<T>::name, str, fmt::join(describe::enum_names<E>(), ", "));
        field = res;
    } else if constexpr (std::rank_v<typename Member::type>) {
        if (val.type() != QVariant::List && val.type() != QVariant::StringList) {
            Raise("{}: {}: Expected a list, got: {}", frame, describe::Get<T>::name, TypeNameOf(val));
        }
        const auto list = val.toList();
        if (size_t(list.size()) != std::size(field))
            Raise("{}: {}: Expected a list of size {}, got: {}", frame, describe::Get<T>::name, std::size(field), list.size());
        uint32_t idx = 0;
        for (auto& v: list) {
            auto i = idx++;
            from_variant(v, field[i], TraceFrame(i, frame));
        }
    } else {
        from_variant(val, field, frame);
    }
}

template<typename T>
void from_variant(QVariant const& in, T& out, [[maybe_unused]] const TraceFrame& frame)
{
    if constexpr (!describe::is_described_v<T>)
    {
        out = in.value<T>();
    }
    else if constexpr (describe::has_v<TypeIsUnionBase, T>)
    {
        using UnionEnum = typename describe::extract_t<TypeIsUnionBase, T>::type;
        if (in.type() != QVariant::Map) {
            Raise("{}: type {}: map expected", frame, describe::Get<T>::name);
        }
        auto* map = reinterpret_cast<const QVariantMap*>(in.constData());
        if (map->size() != 1)
            Raise("{}: type {}: expected union (map with single item). Map has {} elems", frame, describe::Get<T>::name, map->size());
        auto tag = map->constBegin().key();
        auto tag_str = tag.toLatin1();
        UnionEnum etag;
        if (!describe::name_to_enum({tag_str.data(), size_t(tag_str.size())}, etag))
            Raise("{}: type {}: tag is invalid value: {}. Valid: [{}]", frame, describe::Get<T>::name, tag, fmt::join(describe::enum_names<T>(), ", "));
        int idx = 0;
        bool ok = false;
        out._tag_ = etag;
        describe::Get<T>::for_each([&](auto info){
            if (idx++ == etag)
            {
                ok = true;
                field_from_variant(*map, info, out, TraceFrame(info.name, frame));
            }
        });
        if (!ok)
            Raise("{}: Error in description of type: {}", frame, describe::Get<T>::name);
    }
    else
    {
        if (in.type() != QVariant::Map) {
            Raise("{}: type {}: map expected", frame, describe::Get<T>::name);
        }
        auto* map = reinterpret_cast<const QVariantMap*>(in.constData());
        describe::Get<T>::for_each([&](auto info){
            field_from_variant(*map, info, out, TraceFrame(info.name, frame));
        });
    }
}

enum NunavutStatus
{
    SUCCESS = NUNAVUT_SUCCESS,
    ERROR_INVALID_ARGUMENT = NUNAVUT_ERROR_INVALID_ARGUMENT,
    ERROR_SERIALIZATION_BUFFER_TOO_SMALL = NUNAVUT_ERROR_SERIALIZATION_BUFFER_TOO_SMALL,
    ERROR_REPRESENTATION_BAD_ARRAY_LENGTH = NUNAVUT_ERROR_REPRESENTATION_BAD_ARRAY_LENGTH,
    ERROR_REPRESENTATION_BAD_UNION_TAG = NUNAVUT_ERROR_REPRESENTATION_BAD_UNION_TAG,
    ERROR_REPRESENTATION_BAD_DELIMITER_HEADER = NUNAVUT_ERROR_REPRESENTATION_BAD_DELIMITER_HEADER,
};

RAD_DESCRIBE(NunavutStatus)
{
    RAD_ENUM(SUCCESS);
    RAD_ENUM(ERROR_INVALID_ARGUMENT);
    RAD_ENUM(ERROR_SERIALIZATION_BUFFER_TOO_SMALL);
    RAD_ENUM(ERROR_REPRESENTATION_BAD_ARRAY_LENGTH);
    RAD_ENUM(ERROR_REPRESENTATION_BAD_UNION_TAG);
    RAD_ENUM(ERROR_REPRESENTATION_BAD_DELIMITER_HEADER);
}

static std::string_view translate(NunavutStatus status)
{
    std::string_view res = "UNKNOWN_ERROR";
    (void)describe::enum_to_name(status, res);
    return res;
}

template<typename Dummy, typename T, SerializeImpl<T> impl>
static void do_serialize(QVariant const& data, uint8_t* buffer, size_t& size)
{
    T value;
    from_variant(data, value, {});
    int8_t err;
    if ((err = impl(&value, buffer, &size)))
        radapter::Raise("Cyphal: error serializing type: {} -> {}", describe::Get<T>::name, translate(NunavutStatus(err)));
}

template<typename Dummy, typename T, DeserializeImpl<T> impl>
static QVariant do_deserialize(const uint8_t* buffer, size_t size)
{
    T value;
    QVariant res;
    int8_t err;
    if ((err = impl(&value, buffer, &size)))
        radapter::Raise("Cyphal: error deserializing type: {} -> {}", describe::Get<T>::name, translate(NunavutStatus(err)));
    to_variant(value, res, {});
    return res;
}

using radapter::can::CanardMessageDynamic;


auto collect_types(...) -> void;

constexpr int types_begin = __COUNTER__;

#define CYPHAL_TYPE(name, ver, attrs, ...) \
    DESCRIBE(#name, name##_##ver, _OPENVA attrs) \
    { \
        __VA_ARGS__ \
    } \
    template<typename Dummy> \
    constexpr radapter::can::CanardMessageDynamic Dyn_##name##_##ver = { \
        u"" name##_##ver##_FULL_NAME_, \
        u"" name##_##ver##_FULL_NAME_AND_VERSION_, \
        name##_##ver##_EXTENT_BYTES_, \
        static_cast<CanardMessageDynamic::Deserialize>(do_deserialize<Dummy, name##_##ver, name##_##ver##_deserialize_>), \
        static_cast<CanardMessageDynamic::Serialize>(do_serialize<Dummy, name##_##ver, name##_##ver##_serialize_>), \
    }; \
    template<typename Dummy> \
    constexpr const CanardMessageDynamic* get_type(std::integral_constant<size_t, __COUNTER__ - types_begin - 1>) { return &Dyn_##name##_##ver<Dummy>; }


//CYPHAL_TYPES_BEGIN
CYPHAL_TYPE(uavcan_si_unit_angle_Quaternion, 1_0, (void),
    RAD_MEMBER(wxyz);)
CYPHAL_TYPE(uavcan_si_unit_length_WideScalar, 1_0, (void),
    RAD_MEMBER(meter);)
CYPHAL_TYPE(uavcan_si_unit_length_WideVector3, 1_0, (void),
    RAD_MEMBER(meter);)
CYPHAL_TYPE(uavcan_si_unit_duration_WideScalar, 1_0, (void),
    RAD_MEMBER(second);)
CYPHAL_TYPE(uavcan_si_unit_duration_Scalar, 1_0, (void),
    RAD_MEMBER(second);)
CYPHAL_TYPE(uavcan_si_unit_acceleration_Vector3, 1_0, (void),
    RAD_MEMBER(meter_per_second_per_second);)
CYPHAL_TYPE(uavcan_si_unit_acceleration_Scalar, 1_0, (void),
    RAD_MEMBER(meter_per_second_per_second);)
CYPHAL_TYPE(uavcan_si_unit_force_Vector3, 1_0, (void),
    RAD_MEMBER(newton);)
CYPHAL_TYPE(uavcan_si_unit_force_Scalar, 1_0, (void),
    RAD_MEMBER(newton);)
CYPHAL_TYPE(uavcan_si_unit_angular_acceleration_Vector3, 1_0, (void),
    RAD_MEMBER(radian_per_second_per_second);)
CYPHAL_TYPE(uavcan_si_unit_angular_acceleration_Scalar, 1_0, (void),
    RAD_MEMBER(radian_per_second_per_second);)
CYPHAL_TYPE(uavcan_si_unit_magnetic_field_strength_Vector3, 1_0, (void),
    RAD_MEMBER(tesla);)
CYPHAL_TYPE(uavcan_si_unit_magnetic_field_strength_Vector3, 1_1, (void),
    RAD_MEMBER(ampere_per_meter);)
CYPHAL_TYPE(uavcan_si_unit_magnetic_field_strength_Scalar, 1_0, (void),
    RAD_MEMBER(tesla);)
CYPHAL_TYPE(uavcan_si_unit_magnetic_field_strength_Scalar, 1_1, (void),
    RAD_MEMBER(ampere_per_meter);)
CYPHAL_TYPE(uavcan_si_unit_electric_current_Scalar, 1_0, (void),
    RAD_MEMBER(ampere);)
CYPHAL_TYPE(uavcan_si_unit_volume_Scalar, 1_0, (void),
    RAD_MEMBER(cubic_meter);)
CYPHAL_TYPE(uavcan_si_unit_mass_Scalar, 1_0, (void),
    RAD_MEMBER(kilogram);)
CYPHAL_TYPE(uavcan_si_unit_temperature_Scalar, 1_0, (void),
    RAD_MEMBER(kelvin);)
CYPHAL_TYPE(uavcan_si_unit_pressure_Scalar, 1_0, (void),
    RAD_MEMBER(pascal);)
CYPHAL_TYPE(uavcan_si_unit_frequency_Scalar, 1_0, (void),
    RAD_MEMBER(hertz);)
CYPHAL_TYPE(uavcan_si_unit_energy_Scalar, 1_0, (void),
    RAD_MEMBER(joule);)
CYPHAL_TYPE(uavcan_si_unit_angle_Scalar, 1_0, (void),
    RAD_MEMBER(radian);)
CYPHAL_TYPE(uavcan_si_unit_torque_Vector3, 1_0, (void),
    RAD_MEMBER(newton_meter);)
CYPHAL_TYPE(uavcan_si_unit_torque_Scalar, 1_0, (void),
    RAD_MEMBER(newton_meter);)
CYPHAL_TYPE(uavcan_si_unit_electric_charge_Scalar, 1_0, (void),
    RAD_MEMBER(coulomb);)
CYPHAL_TYPE(uavcan_si_unit_magnetic_flux_density_Vector3, 1_0, (void),
    RAD_MEMBER(tesla);)
CYPHAL_TYPE(uavcan_si_unit_magnetic_flux_density_Scalar, 1_0, (void),
    RAD_MEMBER(tesla);)
CYPHAL_TYPE(uavcan_si_unit_angular_velocity_Vector3, 1_0, (void),
    RAD_MEMBER(radian_per_second);)
CYPHAL_TYPE(uavcan_si_unit_angular_velocity_Scalar, 1_0, (void),
    RAD_MEMBER(radian_per_second);)
CYPHAL_TYPE(uavcan_si_unit_power_Scalar, 1_0, (void),
    RAD_MEMBER(watt);)
CYPHAL_TYPE(uavcan_si_unit_length_Vector3, 1_0, (void),
    RAD_MEMBER(meter);)
CYPHAL_TYPE(uavcan_si_unit_length_Scalar, 1_0, (void),
    RAD_MEMBER(meter);)
CYPHAL_TYPE(uavcan_si_unit_volumetric_flow_rate_Scalar, 1_0, (void),
    RAD_MEMBER(cubic_meter_per_second);)
CYPHAL_TYPE(uavcan_si_unit_voltage_Scalar, 1_0, (void),
    RAD_MEMBER(volt);)
CYPHAL_TYPE(uavcan_si_unit_luminance_Scalar, 1_0, (void),
    RAD_MEMBER(candela_per_square_meter);)
CYPHAL_TYPE(uavcan_si_unit_velocity_Vector3, 1_0, (void),
    RAD_MEMBER(meter_per_second);)
CYPHAL_TYPE(uavcan_si_unit_velocity_Scalar, 1_0, (void),
    RAD_MEMBER(meter_per_second);)
CYPHAL_TYPE(uavcan_node_IOStatistics, 0_1, (void),
    RAD_MEMBER(num_emitted);
    RAD_MEMBER(num_errored);
    RAD_MEMBER(num_received);)

enum NodeMode
{
    OPERATIONAL = uavcan_node_Mode_1_0_OPERATIONAL,
    INITIALIZATION = uavcan_node_Mode_1_0_INITIALIZATION,
    MAINTENANCE = uavcan_node_Mode_1_0_MAINTENANCE,
    SOFTWARE_UPDATE = uavcan_node_Mode_1_0_SOFTWARE_UPDATE,
};

RAD_DESCRIBE(NodeMode) {
    RAD_ENUM(OPERATIONAL);
    RAD_ENUM(INITIALIZATION);
    RAD_ENUM(MAINTENANCE);
    RAD_ENUM(SOFTWARE_UPDATE);
}

CYPHAL_TYPE(uavcan_node_Mode, 1_0, (void),
    MEMBER("value", &_::value, FieldIsEnum<NodeMode>);)

CYPHAL_TYPE(uavcan_node_Heartbeat, 1_0, (void),
    RAD_MEMBER(health);
    RAD_MEMBER(mode);
    RAD_MEMBER(uptime);
    RAD_MEMBER(vendor_specific_status_code);)

CYPHAL_TYPE(uavcan_node_ExecuteCommand_Request, 1_0, (void),
    RAD_MEMBER(command);
    MEMBER("parameter", &_::parameter, FieldIsDynString);)
CYPHAL_TYPE(uavcan_node_ExecuteCommand_Response, 1_0, (void),
    RAD_MEMBER(status);)
CYPHAL_TYPE(uavcan_node_GetInfo_Request, 1_0, (void),)

CYPHAL_TYPE(uavcan_node_GetInfo_Response, 1_0, (void),
    RAD_MEMBER(protocol_version);
    RAD_MEMBER(hardware_version);
    RAD_MEMBER(software_version);
    RAD_MEMBER(software_vcs_revision_id);
    MEMBER("unique_id", &_::unique_id, FieldIsUniqueID);
    MEMBER("name", &_::name, FieldIsDynString);
    MEMBER("software_image_crc", &_::software_image_crc, FieldIsDynArray);
    MEMBER("certificate_of_authenticity", &_::certificate_of_authenticity, FieldIsDynString);)

CYPHAL_TYPE(uavcan_node_GetTransportStatistics_Request, 0_1, (void),)
CYPHAL_TYPE(uavcan_node_GetTransportStatistics_Response, 0_1, (void),
    RAD_MEMBER(transfer_statistics);
    MEMBER("network_interface_statistics", &_::network_interface_statistics, FieldIsDynArray);)

CYPHAL_TYPE(uavcan_node_ExecuteCommand_Request, 1_1, (void),
    RAD_MEMBER(command);
    MEMBER("parameter", &_::parameter, FieldIsDynString);)
CYPHAL_TYPE(uavcan_node_ExecuteCommand_Response, 1_1, (void),
    RAD_MEMBER(status);)

CYPHAL_TYPE(uavcan_node_ExecuteCommand_Request, 1_2, (void),
    RAD_MEMBER(command);
    MEMBER("parameter", &_::parameter, FieldIsDynString);)
CYPHAL_TYPE(uavcan_node_ExecuteCommand_Response, 1_2, (void),
    RAD_MEMBER(status);)

CYPHAL_TYPE(uavcan_node_ID, 1_0, (void),
    RAD_MEMBER(value);)

enum SubjectIDListTag
{
    mask_bitpacked,
    sparse_list,
    total,
};

CYPHAL_TYPE(uavcan_node_port_ServiceIDList, 1_0, (void),
    MEMBER("mask_bitpacked", &_::mask_bitpacked_, FieldIsBitmask);)


RAD_DESCRIBE(SubjectIDListTag) {
    RAD_ENUM(mask_bitpacked);
    RAD_ENUM(sparse_list);
    RAD_ENUM(total);
}

CYPHAL_TYPE(uavcan_node_port_SubjectIDList, 1_0, (TypeIsUnion<SubjectIDListTag>),
    MEMBER("mask_bitpacked", &_::mask_bitpacked_, FieldIsBitmask);
    MEMBER("sparse_list", &_::sparse_list, FieldIsDynArray);
    MEMBER("total", &_::_total);)

enum PortIDTag {
    subject_id,
    service_id,
};

RAD_DESCRIBE(PortIDTag) {
    RAD_ENUM(subject_id);
    RAD_ENUM(service_id);
}

CYPHAL_TYPE(uavcan_node_port_ID, 1_0, (TypeIsUnion<PortIDTag>),
    RAD_MEMBER(subject_id);
    RAD_MEMBER(service_id);)
CYPHAL_TYPE(uavcan_node_port_ServiceID, 1_0, (void),
    RAD_MEMBER(value);)
CYPHAL_TYPE(uavcan_node_port_SubjectID, 1_0, (void),
    RAD_MEMBER(value);)
CYPHAL_TYPE(uavcan_node_port_List, 1_0, (void),
    RAD_MEMBER(clients);
    RAD_MEMBER(publishers);
    RAD_MEMBER(servers);
    RAD_MEMBER(subscribers);)
CYPHAL_TYPE(uavcan_node_Version, 1_0, (void),
    RAD_MEMBER(major);
    RAD_MEMBER(minor);)

#define CyphalEnum()

enum NodeHealth {
    NOMINAL = uavcan_node_Health_1_0_NOMINAL,
    ADVISORY = uavcan_node_Health_1_0_ADVISORY,
    CAUTION = uavcan_node_Health_1_0_CAUTION,
    WARNING = uavcan_node_Health_1_0_WARNING,
};

RAD_DESCRIBE(NodeHealth) {
    RAD_ENUM(NOMINAL);
    RAD_ENUM(ADVISORY);
    RAD_ENUM(CAUTION);
    RAD_ENUM(WARNING);
}

CYPHAL_TYPE(uavcan_node_Health, 1_0, (void),
    MEMBER("value", &_::value, FieldIsEnum<NodeHealth>);)
CYPHAL_TYPE(uavcan_time_Synchronization, 1_0, (void),
    RAD_MEMBER(previous_transmission_timestamp_microsecond);)

CYPHAL_TYPE(uavcan_time_GetSynchronizationMasterInfo_Request, 0_1, (void),)
CYPHAL_TYPE(uavcan_time_GetSynchronizationMasterInfo_Response, 0_1, (void),
    RAD_MEMBER(tai_info);
    RAD_MEMBER(time_system);
    RAD_MEMBER(error_variance);)

CYPHAL_TYPE(uavcan_time_TAIInfo, 0_1, (void),
    RAD_MEMBER(difference_tai_minus_utc);)
CYPHAL_TYPE(uavcan_time_SynchronizedTimestamp, 1_0, (void),
    RAD_MEMBER(microsecond);)


enum TimeSystem {
    MONOTONIC_SINCE_BOOT = uavcan_time_TimeSystem_0_1_MONOTONIC_SINCE_BOOT,
    TAI = uavcan_time_TimeSystem_0_1_TAI,
    APPLICATION_SPECIFIC = uavcan_time_TimeSystem_0_1_APPLICATION_SPECIFIC,
};

RAD_DESCRIBE(TimeSystem) {
    RAD_ENUM(MONOTONIC_SINCE_BOOT);
    RAD_ENUM(TAI);
    RAD_ENUM(APPLICATION_SPECIFIC);
}

CYPHAL_TYPE(uavcan_time_TimeSystem, 0_1, (void),
    MEMBER("value", &_::value, FieldIsEnum<TimeSystem>);)

CYPHAL_TYPE(uavcan_primitive_scalar_Real64, 1_0, (void),
    RAD_MEMBER(value);)
CYPHAL_TYPE(uavcan_primitive_scalar_Natural64, 1_0, (void),
    RAD_MEMBER(value);)
CYPHAL_TYPE(uavcan_primitive_scalar_Integer8, 1_0, (void),
    RAD_MEMBER(value);)
CYPHAL_TYPE(uavcan_primitive_scalar_Integer32, 1_0, (void),
    RAD_MEMBER(value);)
CYPHAL_TYPE(uavcan_primitive_scalar_Bit, 1_0, (void),
    RAD_MEMBER(value);)
CYPHAL_TYPE(uavcan_primitive_scalar_Integer16, 1_0, (void),
    RAD_MEMBER(value);)
CYPHAL_TYPE(uavcan_primitive_scalar_Integer64, 1_0, (void),
    RAD_MEMBER(value);)
CYPHAL_TYPE(uavcan_primitive_scalar_Real16, 1_0, (void),
    RAD_MEMBER(value);)
CYPHAL_TYPE(uavcan_primitive_scalar_Natural8, 1_0, (void),
    RAD_MEMBER(value);)
CYPHAL_TYPE(uavcan_primitive_scalar_Natural32, 1_0, (void),
    RAD_MEMBER(value);)
CYPHAL_TYPE(uavcan_primitive_scalar_Real32, 1_0, (void),
    RAD_MEMBER(value);)
CYPHAL_TYPE(uavcan_primitive_scalar_Natural16, 1_0, (void),
    RAD_MEMBER(value);)
CYPHAL_TYPE(uavcan_primitive_Empty, 1_0, (void),)

CYPHAL_TYPE(uavcan_primitive_array_Real64, 1_0, (void),
    MEMBER("value", &_::value, FieldIsDynArray);)
CYPHAL_TYPE(uavcan_primitive_array_Natural64, 1_0, (void),
    MEMBER("value", &_::value, FieldIsDynArray);)
CYPHAL_TYPE(uavcan_primitive_array_Integer8, 1_0, (void),
    MEMBER("value", &_::value, FieldIsDynArray);)
CYPHAL_TYPE(uavcan_primitive_array_Integer32, 1_0, (void),
    MEMBER("value", &_::value, FieldIsDynArray);)
CYPHAL_TYPE(uavcan_primitive_array_Bit, 1_0, (void),
    MEMBER("value", &_::value, FieldIsBitArray);)
CYPHAL_TYPE(uavcan_primitive_array_Integer16, 1_0, (void),
    MEMBER("value", &_::value, FieldIsDynArray);)
CYPHAL_TYPE(uavcan_primitive_array_Integer64, 1_0, (void),
    MEMBER("value", &_::value, FieldIsDynArray);)
CYPHAL_TYPE(uavcan_primitive_array_Real16, 1_0, (void),
    MEMBER("value", &_::value, FieldIsDynArray);)
CYPHAL_TYPE(uavcan_primitive_array_Natural8, 1_0, (void),
    MEMBER("value", &_::value, FieldIsDynArray);)
CYPHAL_TYPE(uavcan_primitive_array_Natural32, 1_0, (void),
    MEMBER("value", &_::value, FieldIsDynArray);)
CYPHAL_TYPE(uavcan_primitive_array_Real32, 1_0, (void),
    MEMBER("value", &_::value, FieldIsDynArray);)
CYPHAL_TYPE(uavcan_primitive_array_Natural16, 1_0, (void),
    MEMBER("value", &_::value, FieldIsDynArray);)
CYPHAL_TYPE(uavcan_primitive_String, 1_0, (void),
    MEMBER("value", &_::value, FieldIsDynString);)
CYPHAL_TYPE(uavcan_primitive_Unstructured, 1_0, (void),
    MEMBER("value", &_::value, FieldIsDynArray);)

enum RegisterValueTag
{
    empty,
    string,
    unstructured,
    bit,
    integer64,
    integer32,
    integer16,
    integer8,
    natural64,
    natural32,
    natural16,
    natural8,
    real64,
    real32,
    real16,
};

RAD_DESCRIBE(RegisterValueTag) {
    RAD_ENUM(empty);
    RAD_ENUM(string);
    RAD_ENUM(unstructured);
    RAD_ENUM(bit);
    RAD_ENUM(integer64);
    RAD_ENUM(integer32);
    RAD_ENUM(integer16);
    RAD_ENUM(integer8);
    RAD_ENUM(natural64);
    RAD_ENUM(natural32);
    RAD_ENUM(natural16);
    RAD_ENUM(natural8);
    RAD_ENUM(real64);
    RAD_ENUM(real32);
    RAD_ENUM(real16);
}

CYPHAL_TYPE(uavcan_register_Value, 1_0, (TypeIsUnion<RegisterValueTag>),
    RAD_MEMBER(empty);
    MEMBER("string", &_::_string);
    RAD_MEMBER(unstructured);
    RAD_MEMBER(bit);
    RAD_MEMBER(integer64);
    RAD_MEMBER(integer32);
    RAD_MEMBER(integer16);
    RAD_MEMBER(integer8);
    RAD_MEMBER(natural64);
    RAD_MEMBER(natural32);
    RAD_MEMBER(natural16);
    RAD_MEMBER(natural8);
    RAD_MEMBER(real64);
    RAD_MEMBER(real32);
    RAD_MEMBER(real16);
)
CYPHAL_TYPE(uavcan_register_Access_Request, 1_0, (void),
    RAD_MEMBER(name);
    RAD_MEMBER(value);)
CYPHAL_TYPE(uavcan_register_Access_Response, 1_0, (void),)
CYPHAL_TYPE(uavcan_register_Name, 1_0, (void),
    MEMBER("name", &_::name, FieldIsDynString);)
CYPHAL_TYPE(uavcan_register_List_Request, 1_0, (void),
    RAD_MEMBER(index);)
CYPHAL_TYPE(uavcan_register_List_Response, 1_0, (void),
    RAD_MEMBER(name);)
//CYPHAL_TYPES_END

constexpr int types_end = __COUNTER__ - types_begin - 1;

template<size_t I>
constexpr void add_types(const CanardMessageDynamic** dyns, std::integral_constant<size_t, I>) {
    if constexpr(I < types_end) {
        dyns[I] = get_type<void>(std::integral_constant<size_t, I>{});
        add_types(dyns, std::integral_constant<size_t, I + 1>{});
    }
}

namespace radapter::can
{

struct CyphalGlobal
{
    QMap<QStringView, const CanardMessageDynamic*> by_name;
    QMap<QStringView, const CanardMessageDynamic*> by_name_and_ver;

    static CyphalGlobal& get() {
        static CyphalGlobal state;
        return state;
    }
};

constexpr std::array<const CanardMessageDynamic*, types_end> get_all_types() {
    std::array<const CanardMessageDynamic*, types_end> res{};
    add_types(res.data(), std::integral_constant<size_t, 0>{});
    return res;
}

constexpr auto all_types = get_all_types();

static bool do_init_msgs(const CanardMessageDynamic* const* dyns, size_t count)
{
    auto& state = CyphalGlobal::get();
    for (size_t i = 0; i < count; ++i) {
        auto* dyn = dyns[i];
        state.by_name[dyn->name] = dyn;
        state.by_name_and_ver[dyn->name_and_ver] = dyn;
    }
    return true;
}

const CanardMessageDynamic* lookup_canard_type(QStringView name)
{
    auto& state = CyphalGlobal::get();
    [[maybe_unused]] static bool _ = do_init_msgs(all_types.data(), all_types.size());
    const CanardMessageDynamic* res = nullptr;
    (void)((res = state.by_name.value(name)) || (res = state.by_name_and_ver.value(name)));
    return res;
}

}
