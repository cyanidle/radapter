#pragma once

#include <QVariant>
#include <QVariantMap>
#include <QVariantList>
#include <QString>
#include <cstdint>
#include <string>
#include <cstring>  
#include <memory_resource>
#include <stdexcept>
#include <json_view/alloc.hpp>

#include "rosidl_typesupport_introspection_cpp/message_introspection.hpp"
#include "rosidl_typesupport_introspection_cpp/field_types.hpp"


namespace ros_introspection = rosidl_typesupport_introspection_cpp;

static QVariant extract_field(const void* field_ptr, uint8_t type_id);
static void pack_field(void* field_ptr, uint8_t type_id, const QVariant& value);

static QVariant ros_to_qt(const void* msg, const ros_introspection::MessageMembers* typesupport)
{
    QVariantMap message_map;

    for (uint32_t i = 0; i < typesupport->member_count_; ++i) {
        const auto& member = typesupport->members_[i];
        const void* member_ptr = static_cast<const uint8_t*>(msg) + member.offset_;
        QString field_name = QString::fromUtf8(member.name_);

        if (member.is_array_) {
            QVariantList q_list;
            size_t array_size = 0;

            if (member.size_function) {
                array_size = member.size_function(member_ptr);
            } else {
                array_size = member.array_size_;
            }

            for (size_t j = 0; j < array_size; ++j) {
                const void* element_ptr = nullptr;
                if (member.get_const_function) {
                    element_ptr = member.get_const_function(member_ptr, j);
                } else {
                    // Fallback for flat types if no get function pointer exists
                    // Calculate memory footprint based on index
                     throw std::runtime_error("Array accessor functions missing for field: " + std::string(member.name_));
                }

                if (member.type_id_ == ros_introspection::ROS_TYPE_MESSAGE) {
                    auto sub_members = static_cast<const ros_introspection::MessageMembers*>(member.members_->data);
                    q_list.append(ros_to_qt(element_ptr, sub_members));
                } else {
                    q_list.append(extract_field(element_ptr, member.type_id_));
                }
            }
            message_map[field_name] = q_list;
        } 
        // Handle Nested Message Fields
        else if (member.type_id_ == ros_introspection::ROS_TYPE_MESSAGE) {
            auto sub_members = static_cast<const ros_introspection::MessageMembers*>(member.members_->data);
            message_map[field_name] = ros_to_qt(member_ptr, sub_members);
        } 
        // Handle Primitive/Basic Fields
        else {
            message_map[field_name] = extract_field(member_ptr, member.type_id_);
        }
    }

    return QVariant(message_map);
}

// Helper to cast memory addresses to standard Qt primitives
static QVariant extract_field(const void* field_ptr, uint8_t type_id)
{
    switch (type_id) {
        case ros_introspection::ROS_TYPE_BOOLEAN: return *static_cast<const bool*>(field_ptr);
        case ros_introspection::ROS_TYPE_BYTE:    return *static_cast<const uint8_t*>(field_ptr);
        case ros_introspection::ROS_TYPE_CHAR:    return *static_cast<const char*>(field_ptr);
        case ros_introspection::ROS_TYPE_FLOAT:   return *static_cast<const float*>(field_ptr);
        case ros_introspection::ROS_TYPE_DOUBLE:  return *static_cast<const double*>(field_ptr);
        case ros_introspection::ROS_TYPE_INT8:    return *static_cast<const int8_t*>(field_ptr);
        case ros_introspection::ROS_TYPE_UINT8:   return *static_cast<const uint8_t*>(field_ptr);
        case ros_introspection::ROS_TYPE_INT16:   return *static_cast<const int16_t*>(field_ptr);
        case ros_introspection::ROS_TYPE_UINT16:  return *static_cast<const uint16_t*>(field_ptr);
        case ros_introspection::ROS_TYPE_INT32:   return *static_cast<const int32_t*>(field_ptr);
        case ros_introspection::ROS_TYPE_UINT32:  return *static_cast<const uint32_t*>(field_ptr);
        case ros_introspection::ROS_TYPE_INT64:   return qint64(*static_cast<const int64_t*>(field_ptr));
        case ros_introspection::ROS_TYPE_UINT64:  return quint64(*static_cast<const uint64_t*>(field_ptr));
        case ros_introspection::ROS_TYPE_STRING: {
            const auto* str = static_cast<const std::string*>(field_ptr);
            return QString::fromStdString(*str);
        }
        default: return QVariant();
    }
}

static void qt_to_ros(void* ros_msg, const QVariant& msg, const ros_introspection::MessageMembers* typesupport)
{
    QVariantMap source_map = msg.toMap();

    for (uint32_t i = 0; i < typesupport->member_count_; ++i) {
        const auto& member = typesupport->members_[i];
        void* member_ptr = static_cast<uint8_t*>(ros_msg) + member.offset_;
        QString field_name = QString::fromUtf8(member.name_);

        if (!source_map.contains(field_name)) continue;
        QVariant field_val = source_map[field_name];

        if (member.is_array_) {
            if (!field_val.canConvert<QVariantList>()) continue;
            QVariantList q_list = field_val.toList();
            size_t target_size = q_list.size();

            // Resize structural dynamic vector representations
            if (member.resize_function) {
                member.resize_function(member_ptr, target_size);
            }

            for (size_t j = 0; j < target_size; ++j) {
                void* element_ptr = member.get_function(member_ptr, j);

                if (member.type_id_ == ros_introspection::ROS_TYPE_MESSAGE) {
                    auto sub_members = static_cast<const ros_introspection::MessageMembers*>(member.members_->data);
                    qt_to_ros(element_ptr, q_list[j], sub_members);
                } else {
                    pack_field(element_ptr, member.type_id_, q_list[j]);
                }
            }
        } 
        else if (member.type_id_ == ros_introspection::ROS_TYPE_MESSAGE) {
            auto sub_members = static_cast<const ros_introspection::MessageMembers*>(member.members_->data);
            qt_to_ros(member_ptr, field_val, sub_members);
        } 
        else {
            pack_field(member_ptr, member.type_id_, field_val);
        }
    }
}

// Helper to write QVariant payloads safely back into raw memory offsets
static void pack_field(void* field_ptr, uint8_t type_id, const QVariant& value)
{
    switch (type_id) {
        case ros_introspection::ROS_TYPE_BOOLEAN: *static_cast<bool*>(field_ptr) = value.toBool(); break;
        case ros_introspection::ROS_TYPE_BYTE:    *static_cast<uint8_t*>(field_ptr) = value.value<uint8_t>(); break;
        case ros_introspection::ROS_TYPE_CHAR:    *static_cast<char*>(field_ptr) = value.value<char>(); break;
        case ros_introspection::ROS_TYPE_FLOAT:   *static_cast<float*>(field_ptr) = value.toFloat(); break;
        case ros_introspection::ROS_TYPE_DOUBLE:  *static_cast<double*>(field_ptr) = value.toDouble(); break;
        case ros_introspection::ROS_TYPE_INT8:    *static_cast<int8_t*>(field_ptr) = value.value<int8_t>(); break;
        case ros_introspection::ROS_TYPE_UINT8:   *static_cast<uint8_t*>(field_ptr) = value.value<uint8_t>(); break;
        case ros_introspection::ROS_TYPE_INT16:   *static_cast<int16_t*>(field_ptr) = value.value<int16_t>(); break;
        case ros_introspection::ROS_TYPE_UINT16:  *static_cast<uint16_t*>(field_ptr) = value.value<uint16_t>(); break;
        case ros_introspection::ROS_TYPE_INT32:   *static_cast<int32_t*>(field_ptr) = value.toInt(); break;
        case ros_introspection::ROS_TYPE_UINT32:  *static_cast<uint32_t*>(field_ptr) = value.toUInt(); break;
        case ros_introspection::ROS_TYPE_INT64:   *static_cast<int64_t*>(field_ptr) = value.toLongLong(); break;
        case ros_introspection::ROS_TYPE_UINT64:  *static_cast<uint64_t*>(field_ptr) = value.toULongLong(); break;
        case ros_introspection::ROS_TYPE_STRING: {
            auto* str = static_cast<std::string*>(field_ptr);
            *str = value.toString().toStdString();
            break;
        }
        default: break;
    }
}
