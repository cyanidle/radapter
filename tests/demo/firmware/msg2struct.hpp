#pragma once
#ifndef MSG2STRUCT_HPP
#define MSG2STRUCT_HPP

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#if __has_include(<endian.h>)
    #include <endian.h>
#elif !defined(__BYTE_ORDER) || !defined(__LITTLE_ENDIAN)
    #error "__BYTE_ORDER and __LITTLE_ENDIAN must be defined"
#endif

#ifndef MSG_2_STRUCT_EXT //support for ext msgpack types
#define MSG_2_STRUCT_EXT 1
#endif

#ifndef MSG_2_STRUCT_FIXEXT //support for fixext msgpack types
#define MSG_2_STRUCT_FIXEXT 1
#endif

#define _ALWAYS_INLINE __attribute__((always_inline))
#define _FLATTEN __attribute__((flatten))

namespace msg2struct
{

enum class Headers {
    never_used, //  11000001 	0xc1
    invalid = never_used,
// Fix (inline size or data)
    fixuint,    //  0xxxx       0x00 - 0x7f,
    fixint,     //  111xxxx     0xe0 - 0xff,
    fixmap,    // 	1000xxxx 	0x80 - 0x8f
    fixarray,  // 	1001xxxx 	0x90 - 0x9f
    fixstr,    // 	101xxxxx 	0xa0 - 0xbf
// Trivial
    nil,        // 	11000000 	0xc0
    hfalse,     // 	11000010 	0xc2
    htrue,      // 	11000011 	0xc3
// Floats
    float32,    // 	11001010 	0xca
    float64,    // 	11001011 	0xcb
// Unsigned Ints
    uint8,      // 	11001100 	0xcc
    uint16,     // 	11001101 	0xcd
    uint32,     // 	11001110 	0xce
    uint64,     // 	11001111 	0xcf
// Ints
    int8,       // 	11010000 	0xd0
    int16,      // 	11010001 	0xd1
    int32,      // 	11010010 	0xd2
    int64,      // 	11010011 	0xd3
// Containers
    array16,    // 	11011100 	0xdc
    array32,    // 	11011101 	0xdd
    map16,      // 	11011110 	0xde
    map32,      // 	11011111 	0xdf
// Fixext
    fixext1,    // 	11010100 	0xd4
    fixext2,    // 	11010101 	0xd5
    fixext4,    // 	11010110 	0xd6
    fixext8,    // 	11010111 	0xd7
    fixext16,   // 	11011000 	0xd8
// Composites
    bin8,       // 	11000100 	0xc4
    bin16,      // 	11000101 	0xc5
    bin32,      // 	11000110 	0xc6

    ext8,       // 	11000111 	0xc7
    ext16,      // 	11001000 	0xc8
    ext32,      // 	11001001 	0xc9

    str8,       // 	11011001 	0xd9
    str16,      // 	11011010 	0xda
    str32,      // 	11011011 	0xdb
};

struct Binary {
    const unsigned char* data;
    size_t size;

    explicit operator bool() const noexcept {
        return data;
    }
};

struct String {
    const char* str;
    size_t size;

    explicit operator bool() const noexcept {
        return str;
    }
};


template<typename T> struct is_int {};
template<> struct is_int<int8_t> { using type = int; };
template<> struct is_int<uint8_t> { using type = int; };
template<> struct is_int<int16_t> { using type = int; };
template<> struct is_int<uint16_t> { using type = int; };
template<> struct is_int<int32_t> { using type = int; };
template<> struct is_int<uint32_t> { using type = int; };
template<> struct is_int<int64_t> { using type = int; };
template<> struct is_int<uint64_t> { using type = int; };
template<bool> struct check {};
template<> struct check<true> { using type = int; };

namespace impl {


inline Headers header(unsigned char byte) {
    switch (byte) {
    case 0xc0: return Headers::nil;
    case 0xc1: return Headers::never_used;
    case 0xc2: return Headers::hfalse;
    case 0xc3: return Headers::htrue;
    case 0xc4: return Headers::bin8;
    case 0xc5: return Headers::bin16;
    case 0xc6: return Headers::bin32;
    case 0xc7: return Headers::ext8;
    case 0xc8: return Headers::ext16;
    case 0xc9: return Headers::ext32;
    case 0xca: return Headers::float32;
    case 0xcb: return Headers::float64;
    case 0xcc: return Headers::uint8;
    case 0xcd: return Headers::uint16;
    case 0xce: return Headers::uint32;
    case 0xcf: return Headers::uint64;
    case 0xd0: return Headers::int8;
    case 0xd1: return Headers::int16;
    case 0xd2: return Headers::int32;
    case 0xd3: return Headers::int64;
    case 0xd4: return Headers::fixext1;
    case 0xd5: return Headers::fixext2;
    case 0xd6: return Headers::fixext4;
    case 0xd7: return Headers::fixext8;
    case 0xd8: return Headers::fixext16;
    case 0xd9: return Headers::str8;
    case 0xda: return Headers::str16;
    case 0xdb: return Headers::str32;
    case 0xdc: return Headers::array16;
    case 0xdd: return Headers::array32;
    case 0xde: return Headers::map16;
    case 0xdf: return Headers::map32;
    default:
        if (0x00 <= byte && byte <= 0x7f) return Headers::fixuint;
        if (0xe0 <= byte && byte <= 0xff) return Headers::fixint;
        if (0x80 <= byte && byte <= 0x8f) return Headers::fixmap;
        if (0x90 <= byte && byte <= 0x9f) return Headers::fixarray;
        if (0xa0 <= byte && byte <= 0xbf) return Headers::fixstr;
        abort();
    }
}

inline size_t nonCompositeSizeof(Headers type) {
    switch (type) {
    default:
        return 1; //trivial 1 byte (inline or fix*)
    case Headers::bin8:
    case Headers::ext8:
    case Headers::str8:
    case Headers::uint8:
    case Headers::int8:
    case Headers::fixext1:
        return 2; //type + trivial 1-byte
    case Headers::bin16:
    case Headers::ext16:
    case Headers::uint16:
    case Headers::int16:
    case Headers::str16:
    case Headers::array16:
    case Headers::map16:
    case Headers::fixext2:
        return 3; //type + trivial 2-byte
    case Headers::str32:
    case Headers::array32:
    case Headers::bin32:
    case Headers::ext32:
    case Headers::map32:
    case Headers::float32:
    case Headers::uint32:
    case Headers::int32:
    case Headers::fixext4:
        return 5; //type + trivial 4-byte
    case Headers::uint64:
    case Headers::int64:
    case Headers::float64:
    case Headers::fixext8:
        return 9; //type + trivial 8-byte
    case Headers::fixext16:
        return 17; //type + trivial 16-byte
    }
}

template<typename T, typename check<sizeof(T) == 1>::type = 1>
_ALWAYS_INLINE inline T bswap(T val) noexcept {return val;}

template<typename I, typename T>
_ALWAYS_INLINE inline T do_bswap(T val, I temp) noexcept {
    ::memcpy(&temp, &val, sizeof(temp));
    temp = __builtin_bswap16(temp);
    ::memcpy(&val, &temp, sizeof(temp));
    return val;
}

template<typename T, typename check<sizeof(T) == 2>::type = 1>
_ALWAYS_INLINE inline T bswap(T val) noexcept {return do_bswap(val, uint16_t{});}
template<typename T, typename check<sizeof(T) == 4>::type = 1>
_ALWAYS_INLINE inline T bswap(T val) noexcept {return do_bswap(val, uint32_t{});}
template<typename T, typename check<sizeof(T) == 8>::type = 1>
_ALWAYS_INLINE inline T bswap(T val) noexcept {return do_bswap(val, uint64_t{});}

template<typename T>
_ALWAYS_INLINE inline T from_big(T val) {
#if __BYTE_ORDER == __LITTLE_ENDIAN
    return bswap(val);
#endif
}

template<typename T>
_ALWAYS_INLINE inline T to_big(T val) {
#if __BYTE_ORDER == __LITTLE_ENDIAN
    return bswap(val);
#endif
}

template<typename T>
void getTrivial(T& v, const unsigned char* data) {
    memcpy(&v, data, sizeof(v));
    v = from_big(v);
}

template<typename T, typename U>
bool getTrivialWith(U& out, const unsigned char* data, size_t size) {
    T v;
    if (size < (sizeof(v) + 1)) return false;
    impl::getTrivial(v, data + 1);
    out = T(v);
    if ((out > 0) != (v > 0)) return false; //sign missmatch
    if (sizeof(T) > sizeof(out)) {
        if (v != T(out)) return false; //overflow
    }
    return true;
}

template<typename SizeT, size_t add = 0>
void getComposite(Binary& result, const unsigned char* data, size_t size) {
    SizeT sz;
    if (size < 1 + sizeof(sz) + add) return;
    impl::getTrivial(sz, data + 1);
    if (size < 1 + sizeof(sz) + sz + add) return;
    result.data = data + sizeof(sz) + 1 + add;
    result.size = sz + add;
}

} //impl

class InIterator {
    const unsigned char* data;
    size_t size;
    Headers type;
public:
    const unsigned char* Data() const noexcept {
        return data;
    }

    Headers Type() const noexcept {
        return type;
    }

    InIterator() noexcept : type(Headers::invalid) {}

    static InIterator InvalidAt(const unsigned char* point) noexcept {
        InIterator it;
        it.data = point;
        return it;
    }

    InIterator(const unsigned char* _data, size_t _size) noexcept : data(_data), size(_size) {
        type = _size ? impl::header(_data[0]) : Headers::invalid;
    }

    _FLATTEN Binary GetComposite() noexcept {
        Binary result{};
        switch(type) {
        case Headers::bin32:
        case Headers::str32: {
            impl::getComposite<uint32_t>(result, data, size);
            break;
        }
        case Headers::bin16:
        case Headers::str16: {
            impl::getComposite<uint16_t>(result, data, size);
            break;
        }
        case Headers::bin8:
        case Headers::str8: {
            impl::getComposite<uint8_t>(result, data, size);
            break;
        }
#if MSG_2_STRUCT_EXT
        case Headers::ext8: {
            impl::getComposite<uint8_t, 1>(result, data, size);
            break;
        }
        case Headers::ext16: {
            impl::getComposite<uint16_t, 1>(result, data, size);
            break;
        }
        case Headers::ext32: {
            impl::getComposite<uint32_t, 1>(result, data, size);
            break;
        }
#endif
#if MSG_2_STRUCT_FIXEXT
        //TODO
#endif
        case Headers::fixstr: {
            size_t len = data[0] & 31;
            if (size < len) return result;
            result.data = data + 1;
            result.size = len;
            break;
        }
        default:
            break;
        }
        if (result) {
            auto step = impl::nonCompositeSizeof(type) + result.size;
            advance(step);
        }
        return result;
    }

    bool GetBool(bool& out) noexcept {
        auto ok = type == Headers::htrue ? (out = true)
               : type == Headers::hfalse ? !(out = false)
                                         : false;
        if (ok) advance(1);
        return ok;
    }

    bool GetNil() noexcept {
        bool ok = type == Headers::nil;
        if (ok) advance(1);
        return ok;
    }

    template<typename T>
    _FLATTEN bool GetInteger(T& out) noexcept {
        bool ok = false;
        switch(type) {
        case Headers::fixuint: {
            out = T(uint8_t(data[0]));
            ok = true;
            break;
        }
        case Headers::fixint: {
            auto v = int8_t(data[0]);
            out = T(v);
            if ((out > 0) != (v > 0)) return false;
            ok = true;
            break;
        }
        case Headers::uint8: {
            ok = impl::getTrivialWith<uint8_t>(out, data, size);
            break;
        }
        case Headers::uint16: {
            ok = impl::getTrivialWith<uint16_t>(out, data, size);
            break;
        }
        case Headers::uint32: {
            ok = impl::getTrivialWith<uint32_t>(out, data, size);
            break;
        }
        case Headers::uint64: {
            ok = impl::getTrivialWith<uint64_t>(out, data, size);
            break;
        }

        case Headers::int8: {
            ok = impl::getTrivialWith<int8_t>(out, data, size);
            break;
        }
        case Headers::int16: {
            ok = impl::getTrivialWith<int16_t>(out, data, size);
            break;
        }
        case Headers::int32: {
            ok = impl::getTrivialWith<int32_t>(out, data, size);
            break;
        }
        case Headers::int64: {
            ok = impl::getTrivialWith<int64_t>(out, data, size);
            break;
        }
        default: break;
        }
        if (ok) {
            advance(impl::nonCompositeSizeof(type));
        }
        return ok;
    }

    template<typename T>
    _FLATTEN bool GetFloat(T& out) noexcept {
        switch(type) {
        case Headers::float32: {
            auto ok = impl::getTrivialWith<float>(out, data, size);
            if (ok) advance(5);
            return ok;
        }
        case Headers::float64: {
            auto ok = impl::getTrivialWith<double>(out, data, size);
            if (ok) advance(9);
            return ok;
        }
        default: {
            return GetInteger(out);
        }
        }
    }

    bool GetArraySize(size_t& res) noexcept {
        switch(type) {
        case Headers::array16: {
            uint16_t sz;
            if (size < 3) return false;
            impl::getTrivial(sz, data);
            advance(3);
            res = size_t(sz);
            return true;
        }
        case Headers::array32: {
            uint32_t sz;
            if (size < 5) return false;
            impl::getTrivial(sz, data);
            advance(5);
            res = size_t(sz);
            return true;
        }
        case Headers::fixarray: {
            if (!size) return false;
            res = size_t(data[0] & 15);
            advance(1);
            return true;
        }
        default:
            return false;
        }
    }

    bool IsValid() const noexcept {
        return type != Headers::invalid;
    }

    bool Next() noexcept {
        if (!IsValid()) return false;
        auto step = impl::nonCompositeSizeof(type) + GetComposite().size;
        advance(step);
        return true;
    }
private:
    void advance(size_t step) {
        if (step >= size) {
            type = Headers::invalid;
        } else {
            size -= step;
            data += step;
            type = impl::header(data[0]);
        }
    }
};

namespace impl {

template<typename T>
void writeTrivialUnch(T val, unsigned char*& buffer, size_t& size) {
    val = to_big(val);
    memcpy(buffer, &val, sizeof(T));
    buffer += sizeof(T);
    size -= sizeof(T);
}

inline bool writeType(unsigned char type, unsigned char*& buffer, size_t& size) {
    constexpr int step = 1;
    if (size < step) return false;
    buffer[0] = type;
    buffer += 1;
    size -= 1;
    return true;
}

template<typename T>
bool writeTypedTrivial(unsigned char type, T val, unsigned char*& buffer, size_t& size) {
    constexpr int step = sizeof(T) + 1;
    if (size < step) return false;
    buffer[0] = type;
    buffer += 1;
    size -= 1;
    writeTrivialUnch(val, buffer, size);
    return true;
}

_ALWAYS_INLINE _FLATTEN
inline bool writeNegInt(int64_t i, unsigned char*& buffer, size_t& size) {
    if (i >= -32) {
        if (!size) return false;
        buffer[0] = (unsigned char)(int8_t(i));
        buffer += 1;
        size -= 1;
        return true;
    }
    else if (i >= INT8_MIN)
        return writeTypedTrivial(0xD0, int8_t(i), buffer, size);
    else if (i >= INT16_MIN)
        return writeTypedTrivial(0xD1, int16_t(i), buffer, size);
    else if (i >= INT32_MIN)
        return writeTypedTrivial(0xD2, int32_t(i), buffer, size);
    else
        return writeTypedTrivial(0xD3, int64_t(i), buffer, size);
}

_ALWAYS_INLINE _FLATTEN
inline bool writePosInt(uint64_t i, unsigned char*& buffer, size_t& size) {
    if (i < 128) {
        if (!size) return false;
        buffer[0] = (unsigned char)(uint8_t(i));
        buffer += 1;
        size -= 1;
        return true;
    }
    else if (i <= UINT8_MAX)
        return writeTypedTrivial(0xCC, uint8_t(i), buffer, size);
    else if (i <= UINT16_MAX)
        return writeTypedTrivial(0xCD, uint16_t(i), buffer, size);
    else if (i <= UINT32_MAX)
        return writeTypedTrivial(0xCE, uint32_t(i), buffer, size);
    else
        return writeTypedTrivial(0xCF, uint64_t(i), buffer, size);
}

template<int _8, int _16, int _32>
inline bool writeComposite(Binary comp, unsigned char*& buffer, size_t& size) {
    if (comp.size <= UINT8_MAX) {
        if (!writeTypedTrivial(_8, uint8_t(comp.size), buffer, size)) {
            return false;
        }
    } else if (comp.size <= UINT16_MAX) {
        if (!writeTypedTrivial(_16, uint16_t(comp.size), buffer, size)) {
            return false;
        }
    } else {
        if (!writeTypedTrivial(_32, uint32_t(comp.size), buffer, size)) {
            return false;
        }
    }
    if (size < comp.size) return false;
    memcpy(buffer, comp.data, comp.size);
    buffer += comp.size;
    size -= comp.size;
    return true;
}


}

class OutIterator {
public:
    unsigned char* buffer;
    size_t size;
    size_t initial;

    OutIterator(unsigned char* _buffer, size_t size) noexcept :
        buffer(_buffer), size(size), initial(size)
    {}

    size_t Written() const noexcept {
        return initial - size;
    }

    bool BeginArray(size_t _sz) noexcept {
        if (_sz <= 15) {
            if (!size) return false;
            size -= 1;
            buffer[0] = 0x90 | _sz;
            buffer += 1;
            return true;
        } else if (size <= UINT16_MAX) {
            return impl::writeTypedTrivial(0xdc, uint16_t(_sz), buffer, size);
        } else {
            return impl::writeTypedTrivial(0xdd, uint32_t(_sz), buffer, size);
        }
    }

    bool WriteComposite(Binary bin) noexcept {
        return impl::writeComposite<0xc4, 0xc5, 0xc6>(bin, buffer, size);
    }
    bool WriteComposite(String str) noexcept {
        Binary b{};
        b.data = (const unsigned char*)str.str;
        b.size = str.size;
        if (b.size <= 31) { //fixstr
            if (size < 1 + b.size) return false;
            buffer[0] = 0xA0 | b.size;
            buffer += 1;
            size -= 1;
            memcpy(buffer, b.data, b.size);
            buffer += b.size;
            size -= b.size;
            return true;
        } else {
            return impl::writeComposite<0xd9, 0xda, 0xdb>(b, buffer, size);
        }
    }
    bool WriteFloat(float value) noexcept {
        return impl::writeTypedTrivial(0xca, value, buffer, size);
    }
    bool WriteFloat(double value) noexcept {
        return impl::writeTypedTrivial(0xcb, value, buffer, size);
    }
    bool WriteNil() noexcept {
        return impl::writeType(0xc0, buffer, size);
    }
    bool WriteBool(bool value) noexcept {
        return impl::writeType(value ? 0xc2 : 0xc3, buffer, size);
    }
    template<typename T>
    bool WriteInteger(T value) noexcept {
        if (!(value >= 0)) {
            return impl::writeNegInt(int64_t(value), buffer, size);
        } else {
            return impl::writePosInt(uint64_t(value), buffer, size);
        }
    }
};

namespace impl {

template<typename Fn, typename...Args>
_ALWAYS_INLINE inline void apply(Fn& f, Args&&...args) {
    int _helper[] = {(f(args), 0)...};
    (void)_helper;
}

enum {root, child};

#define MSG_2_STRUCT(...) \
static constexpr int is_msg_2_struct = msg2struct::impl::root; \
template<typename Fn> _ALWAYS_INLINE inline void _msg2struct(Fn&& _) const {msg2struct::impl::apply(_, __VA_ARGS__);} \
template<typename Fn> _ALWAYS_INLINE inline void _msg2struct(Fn&& _) {msg2struct::impl::apply(_, __VA_ARGS__);}

#define MSG_2_STRUCT_INHERIT(parent, ...) \
static constexpr int is_msg_2_struct = msg2struct::impl::child; \
using msg_2_parent = parent; \
template<typename Fn> _ALWAYS_INLINE inline void _msg2struct(Fn&& _) const {msg2struct::impl::apply(_, __VA_ARGS__);} \
template<typename Fn> _ALWAYS_INLINE inline void _msg2struct(Fn&& _) {msg2struct::impl::apply(_, __VA_ARGS__);}

struct Counter {
    size_t count{};
    template<typename U>
    void operator()(U&) {count++;}
};

template<typename T, typename check<T::is_msg_2_struct == impl::root>::type = 1>
size_t countFields(T const& val) {
    auto counter = impl::Counter{};
    val._msg2struct(counter);
    return counter.count;
}

template<typename T, typename check<T::is_msg_2_struct == impl::child>::type = 1>
size_t countFields(T const& val) {
    auto& asParent = static_cast<const typename T::msg_2_parent&>(val);
    auto counter = impl::Counter{};
    val._msg2struct(counter);
    return countFields(asParent) + counter.count;
}

struct ParseHelper {
    InIterator& it;
    bool err;
    template<typename U>
    void operator()(U& field) {
        if (err) return;
        if (!Parse(field, it)) {
            err = true;
        }
    }
};
}

// For now only tuple-like parse and dump (we ca use impl::type) to switch
template<typename T, typename check<T::is_msg_2_struct == impl::root>::type = 1>
bool Parse(T& val, InIterator& it, bool fromChild = false) {
    size_t tail = 0;
    if (!fromChild) {
        auto fs = impl::countFields(val);
        size_t sz;
        if (!it.GetArraySize(sz)) return false;
        if (sz < fs) return false;
        tail = sz - fs;
    }
    auto helper = impl::ParseHelper{it, false};
    val._msg2struct(helper);
    if (!helper.err && !fromChild) {
        for (size_t i = 0; i < tail; ++i) it.Next();
    }
    return !helper.err;
}

// Customizable:

template<typename T, typename check<T::is_msg_2_struct == impl::child>::type = 1>
bool Parse(T& val, InIterator& it, bool fromChild = false) {
    size_t tail = 0;
    if (!fromChild) {
        auto fs = impl::countFields(val);
        size_t sz;
        if (!it.GetArraySize(sz)) return false;
        if (sz < fs) return false;
        tail = sz - fs;
    }
    auto& asParent = static_cast<typename T::msg_2_parent&>(val);
    if (!Parse(asParent, it, true)) {
        return false;
    }
    auto helper = impl::ParseHelper{it, false};
    val._msg2struct(helper);
    if (!helper.err && !fromChild) {
        for (size_t i = 0; i < tail; ++i) it.Next();
    }
    return !helper.err;
}

template<typename T, typename is_int<T>::type = 1>
bool Parse(T& val, InIterator& it) {
    return it.GetInteger(val);
}

inline bool Parse(float& val, InIterator& it) {
    return it.GetFloat(val);
}

inline bool Parse(bool& val, InIterator& it) {
    return it.GetBool(val);
}

inline bool Parse(double& val, InIterator& it) {
    return it.GetFloat(val);
}

inline bool Parse(String& val, InIterator& it) {
    switch (it.Type()) {
    case Headers::fixstr:
    case Headers::str8:
    case Headers::str16:
    case Headers::str32: {
        auto bin = it.GetComposite();
        val.size = bin.size;
        val.str = (const char*)bin.data;
        return bool(val);
    }
    default:
        return false;
    }
}

inline bool Parse(Binary& val, InIterator& it) {
    val = it.GetComposite();
    return bool(val);
}

// Dump
namespace impl {

struct DumpHelper {
    OutIterator& it;
    bool err;
    template<typename U>
    void operator()(U& field) {
        if (err) return;
        if (!Dump(field, it)) {
            err = true;
        }
    }
};

}

template<typename T, typename check<T::is_msg_2_struct == impl::root>::type = 1>
bool Dump(T const& val, OutIterator& it, bool fromChild = false) {
    if (!fromChild) {
        auto fs = impl::countFields(val);
        if (!it.BeginArray(fs)) {
            return false;
        }
    }
    auto helper = impl::DumpHelper{it, false};
    val._msg2struct(helper);
    return !helper.err;
}

template<typename T, typename check<T::is_msg_2_struct == impl::child>::type = 1>
bool Dump(T const& val, OutIterator& it, bool fromChild = false) {
    if (!fromChild) {
        auto fs = impl::countFields(val);
        if (!it.BeginArray(fs)) {
            return false;
        }
    }
    auto& asParent = static_cast<const typename T::msg_2_parent&>(val);
    if (!Dump(asParent, it, true)) {
        return false;
    }
    auto helper = impl::DumpHelper{it, false};
    val._msg2struct(helper);
    return !helper.err;
}

template<typename T, typename is_int<T>::type = 1>
bool Dump(T val, OutIterator& it) {
    return it.WriteInteger(val);
}

inline bool Dump(bool val, OutIterator& it) {
    return it.WriteBool(val);
}

inline bool Dump(float val, OutIterator& it) {
    return it.WriteFloat(val);
}

inline bool Dump(double val, OutIterator& it) {
    return it.WriteFloat(val);
}

inline bool Dump(String val, OutIterator& it) {
    return it.WriteComposite(val);
}

inline bool Dump(Binary val, OutIterator& it) {
    return it.WriteComposite(val);
}

} //msg2struct

#endif //MSG2STRUCT_HPP
