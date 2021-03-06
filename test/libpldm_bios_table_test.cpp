#include <string.h>

#include <cstring>
#include <utility>
#include <vector>

#include "libpldm/base.h"
#include "libpldm/bios.h"
#include "libpldm/bios_table.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using testing::ElementsAreArray;
using Table = std::vector<uint8_t>;

void buildTable(Table& table)
{
    auto padSize = ((table.size() % 4) ? (4 - table.size() % 4) : 0);
    table.insert(table.end(), padSize, 0);
    table.insert(table.end(), sizeof(uint32_t) /*checksum*/, 0);
}

template <typename First, typename... Rest>
void buildTable(Table& table, First& first, Rest&... rest)
{
    table.insert(table.end(), first.begin(), first.end());
    buildTable(table, rest...);
}

TEST(AttrTable, EnumEntryDecodeTest)
{
    std::vector<uint8_t> enumEntry{
        0, 0, /* attr handle */
        0,    /* attr type */
        1, 0, /* attr name handle */
        2,    /* number of possible value */
        2, 0, /* possible value handle */
        3, 0, /* possible value handle */
        1,    /* number of default value */
        0     /* defaut value string handle index */
    };

    auto entry =
        reinterpret_cast<struct pldm_bios_attr_table_entry*>(enumEntry.data());
    uint8_t pvNumber = pldm_bios_table_attr_entry_enum_decode_pv_num(entry);
    EXPECT_EQ(pvNumber, 2);
    pvNumber = 0;
    auto rc =
        pldm_bios_table_attr_entry_enum_decode_pv_num_check(entry, &pvNumber);
    EXPECT_EQ(rc, PLDM_SUCCESS);
    EXPECT_EQ(pvNumber, 2);

    std::vector<uint16_t> pvHandles(pvNumber, 0);
    pvNumber = pldm_bios_table_attr_entry_enum_decode_pv_hdls(
        entry, pvHandles.data(), pvHandles.size());
    EXPECT_EQ(pvNumber, 2);
    EXPECT_EQ(pvHandles[0], 2);
    EXPECT_EQ(pvHandles[1], 3);
    pvHandles.resize(1);
    pvNumber = pldm_bios_table_attr_entry_enum_decode_pv_hdls(
        entry, pvHandles.data(), pvHandles.size());
    EXPECT_EQ(pvNumber, 1);
    EXPECT_EQ(pvHandles[0], 2);

    pvHandles.resize(2);
    rc = pldm_bios_table_attr_entry_enum_decode_pv_hdls_check(
        entry, pvHandles.data(), pvHandles.size());
    EXPECT_EQ(rc, PLDM_SUCCESS);
    EXPECT_EQ(pvHandles[0], 2);
    EXPECT_EQ(pvHandles[1], 3);
    rc = pldm_bios_table_attr_entry_enum_decode_pv_hdls_check(
        entry, pvHandles.data(), 1);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_DATA);

    uint8_t defNumber = pldm_bios_table_attr_entry_enum_decode_def_num(entry);
    EXPECT_EQ(defNumber, 1);
    defNumber = 0;
    rc =
        pldm_bios_table_attr_entry_enum_decode_def_num_check(entry, &defNumber);
    EXPECT_EQ(rc, PLDM_SUCCESS);
    EXPECT_EQ(defNumber, 1);

    rc =
        pldm_bios_table_attr_entry_enum_decode_pv_num_check(nullptr, &pvNumber);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_DATA);
    rc = pldm_bios_table_attr_entry_enum_decode_def_num_check(entry, nullptr);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_DATA);

    entry->attr_type = PLDM_BIOS_STRING;
    rc = pldm_bios_table_attr_entry_enum_decode_pv_num_check(entry, &pvNumber);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_DATA);

    rc =
        pldm_bios_table_attr_entry_enum_decode_def_num_check(entry, &defNumber);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_DATA);
    rc =
        pldm_bios_table_attr_entry_enum_decode_pv_hdls_check(entry, nullptr, 0);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_DATA);
}

TEST(AttrTable, EnumEntryEncodeTest)
{
    std::vector<uint8_t> enumEntry{
        0, 0, /* attr handle */
        0,    /* attr type */
        1, 0, /* attr name handle */
        2,    /* number of possible value */
        2, 0, /* possible value handle */
        3, 0, /* possible value handle */
        1,    /* number of default value */
        0     /* defaut value string handle index */
    };

    std::vector<uint16_t> pv_hdls{2, 3};
    std::vector<uint8_t> defs{0};

    struct pldm_bios_table_attr_entry_enum_info info = {
        1,              /* name handle */
        false,          /* read only */
        2,              /* pv number */
        pv_hdls.data(), /* pv handle */
        1,              /*def number */
        defs.data()     /*def index*/
    };
    auto encodeLength = pldm_bios_table_attr_entry_enum_encode_length(2, 1);
    EXPECT_EQ(encodeLength, enumEntry.size());

    std::vector<uint8_t> encodeEntry(encodeLength, 0);
    pldm_bios_table_attr_entry_enum_encode(encodeEntry.data(),
                                           encodeEntry.size(), &info);
    // set attr handle = 0
    encodeEntry[0] = 0;
    encodeEntry[1] = 0;

    EXPECT_EQ(enumEntry, encodeEntry);

    EXPECT_DEATH(pldm_bios_table_attr_entry_enum_encode(
                     encodeEntry.data(), encodeEntry.size() - 1, &info),
                 "length <= entry_length");
    auto rc = pldm_bios_table_attr_entry_enum_encode_check(
        encodeEntry.data(), encodeEntry.size(), &info);
    EXPECT_EQ(rc, PLDM_SUCCESS);
    // set attr handle = 0
    encodeEntry[0] = 0;
    encodeEntry[1] = 0;

    EXPECT_EQ(enumEntry, encodeEntry);
    rc = pldm_bios_table_attr_entry_enum_encode_check(
        encodeEntry.data(), encodeEntry.size() - 1, &info);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_LENGTH);
}

TEST(AttrTable, StringEntryDecodeTest)
{
    std::vector<uint8_t> stringEntry{
        1,   0,       /* attr handle */
        1,            /* attr type */
        12,  0,       /* attr name handle */
        1,            /* string type */
        1,   0,       /* minimum length of the string in bytes */
        100, 0,       /* maximum length of the string in bytes */
        3,   0,       /* length of default string in length */
        'a', 'b', 'c' /* default string  */
    };

    auto entry = reinterpret_cast<struct pldm_bios_attr_table_entry*>(
        stringEntry.data());
    uint16_t def_string_length =
        pldm_bios_table_attr_entry_string_decode_def_string_length(entry);
    EXPECT_EQ(def_string_length, 3);

    def_string_length = 0;
    auto rc = pldm_bios_table_attr_entry_string_decode_def_string_length_check(
        entry, &def_string_length);
    EXPECT_EQ(rc, PLDM_SUCCESS);
    EXPECT_EQ(def_string_length, 3);

    rc = pldm_bios_table_attr_entry_string_decode_def_string_length_check(
        entry, nullptr);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_DATA);
    rc = pldm_bios_table_attr_entry_string_decode_def_string_length_check(
        nullptr, &def_string_length);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_DATA);
    entry->attr_type = PLDM_BIOS_INTEGER;
    rc = pldm_bios_table_attr_entry_string_decode_def_string_length_check(
        entry, &def_string_length);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_DATA);
    rc = pldm_bios_table_attr_entry_string_decode_def_string_length_check(
        nullptr, &def_string_length);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_DATA);
}

TEST(AttrTable, StringEntryEncodeTest)
{
    std::vector<uint8_t> stringEntry{
        0,   0,        /* attr handle */
        1,             /* attr type */
        3,   0,        /* attr name handle */
        1,             /* string type */
        1,   0,        /* min string length */
        100, 0,        /* max string length */
        3,   0,        /* default string length */
        'a', 'b', 'c', /* defaul string */
    };

    struct pldm_bios_table_attr_entry_string_info info = {
        3,     /* name handle */
        false, /* read only */
        1,     /* string type ascii */
        1,     /* min length */
        100,   /* max length */
        3,     /* def length */
        "abc", /* def string */
    };
    auto encodeLength = pldm_bios_table_attr_entry_string_encode_length(3);
    EXPECT_EQ(encodeLength, stringEntry.size());

    std::vector<uint8_t> encodeEntry(encodeLength, 0);
    pldm_bios_table_attr_entry_string_encode(encodeEntry.data(),
                                             encodeEntry.size(), &info);
    // set attr handle = 0
    encodeEntry[0] = 0;
    encodeEntry[1] = 0;

    EXPECT_EQ(stringEntry, encodeEntry);

    EXPECT_DEATH(pldm_bios_table_attr_entry_string_encode(
                     encodeEntry.data(), encodeEntry.size() - 1, &info),
                 "length <= entry_length");
    auto rc = pldm_bios_table_attr_entry_string_encode_check(
        encodeEntry.data(), encodeEntry.size(), &info);
    EXPECT_EQ(rc, PLDM_SUCCESS);
    // set attr handle = 0
    encodeEntry[0] = 0;
    encodeEntry[1] = 0;

    EXPECT_EQ(stringEntry, encodeEntry);
    rc = pldm_bios_table_attr_entry_string_encode_check(
        encodeEntry.data(), encodeEntry.size() - 1, &info);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_LENGTH);
    std::swap(info.max_length, info.min_length);
    const char* errmsg;
    rc = pldm_bios_table_attr_entry_string_info_check(&info, &errmsg);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_DATA);
    EXPECT_STREQ(
        "MinimumStingLength should not be greater than MaximumStringLength",
        errmsg);
    rc = pldm_bios_table_attr_entry_string_encode_check(
        encodeEntry.data(), encodeEntry.size(), &info);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_DATA);
    std::swap(info.max_length, info.min_length);

    std::vector<uint8_t> stringEntryLength0{
        0,   0, /* attr handle */
        1,      /* attr type */
        3,   0, /* attr name handle */
        1,      /* string type */
        1,   0, /* min string length */
        100, 0, /* max string length */
        0,   0, /* default string length */
    };

    info.def_length = 0;
    info.def_string = nullptr;

    encodeLength = pldm_bios_table_attr_entry_string_encode_length(0);
    EXPECT_EQ(encodeLength, stringEntryLength0.size());

    encodeEntry.resize(encodeLength);
    pldm_bios_table_attr_entry_string_encode(encodeEntry.data(),
                                             encodeEntry.size(), &info);
    // set attr handle = 0
    encodeEntry[0] = 0;
    encodeEntry[1] = 0;

    EXPECT_EQ(stringEntryLength0, encodeEntry);
}

TEST(AttrTable, integerEntryEncodeTest)
{
    std::vector<uint8_t> integerEntry{
        0,  0,                   /* attr handle */
        3,                       /* attr type */
        1,  0,                   /* attr name handle */
        1,  0, 0, 0, 0, 0, 0, 0, /* lower bound */
        10, 0, 0, 0, 0, 0, 0, 0, /* upper bound */
        2,  0, 0, 0,             /* scalar increment */
        3,  0, 0, 0, 0, 0, 0, 0, /* defaut value */
    };

    std::vector<uint16_t> pv_hdls{2, 3};
    std::vector<uint8_t> defs{0};

    struct pldm_bios_table_attr_entry_integer_info info = {
        1,     /* name handle */
        false, /* read only */
        1,     /* lower bound */
        10,    /* upper bound */
        2,     /* sacalar increment */
        3      /* default value */
    };
    auto encodeLength = pldm_bios_table_attr_entry_integer_encode_length();
    EXPECT_EQ(encodeLength, integerEntry.size());

    std::vector<uint8_t> encodeEntry(encodeLength, 0);
    pldm_bios_table_attr_entry_integer_encode(encodeEntry.data(),
                                              encodeEntry.size(), &info);
    // set attr handle = 0
    encodeEntry[0] = 0;
    encodeEntry[1] = 0;

    EXPECT_EQ(integerEntry, encodeEntry);

    EXPECT_DEATH(pldm_bios_table_attr_entry_integer_encode(
                     encodeEntry.data(), encodeEntry.size() - 1, &info),
                 "length <= entry_length");

    auto rc = pldm_bios_table_attr_entry_integer_encode_check(
        encodeEntry.data(), encodeEntry.size(), &info);
    EXPECT_EQ(rc, PLDM_SUCCESS);
    // set attr handle = 0
    encodeEntry[0] = 0;
    encodeEntry[1] = 0;

    EXPECT_EQ(integerEntry, encodeEntry);

    rc = pldm_bios_table_attr_entry_integer_encode_check(
        encodeEntry.data(), encodeEntry.size() - 1, &info);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_LENGTH);

    info.lower_bound = 100;
    info.upper_bound = 50;
    const char* errmsg;
    rc = pldm_bios_table_attr_entry_integer_info_check(&info, &errmsg);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_DATA);
    EXPECT_STREQ("LowerBound should not be greater than UpperBound", errmsg);
    rc = pldm_bios_table_attr_entry_integer_encode_check(
        encodeEntry.data(), encodeEntry.size(), &info);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_DATA);
}

TEST(AttrTable, ItearatorTest)
{
    std::vector<uint8_t> enumEntry{
        0, 0, /* attr handle */
        0,    /* attr type */
        1, 0, /* attr name handle */
        2,    /* number of possible value */
        2, 0, /* possible value handle */
        3, 0, /* possible value handle */
        1,    /* number of default value */
        0     /* defaut value string handle index */
    };
    std::vector<uint8_t> stringEntry{
        1,   0,       /* attr handle */
        1,            /* attr type */
        12,  0,       /* attr name handle */
        1,            /* string type */
        1,   0,       /* minimum length of the string in bytes */
        100, 0,       /* maximum length of the string in bytes */
        3,   0,       /* length of default string in length */
        'a', 'b', 'c' /* default string  */
    };
    std::vector<uint8_t> integerEntry{
        0,  0,                   /* attr handle */
        3,                       /* attr type */
        1,  0,                   /* attr name handle */
        1,  0, 0, 0, 0, 0, 0, 0, /* lower bound */
        10, 0, 0, 0, 0, 0, 0, 0, /* upper bound */
        2,  0, 0, 0,             /* scalar increment */
        3,  0, 0, 0, 0, 0, 0, 0, /* defaut value */
    };

    Table table;
    buildTable(table, enumEntry, stringEntry, integerEntry, enumEntry);
    auto iter = pldm_bios_table_iter_create(table.data(), table.size(),
                                            PLDM_BIOS_ATTR_TABLE);
    auto entry = pldm_bios_table_iter_attr_entry_value(iter);
    auto rc = std::memcmp(entry, enumEntry.data(), enumEntry.size());
    EXPECT_EQ(rc, 0);

    pldm_bios_table_iter_next(iter);
    entry = pldm_bios_table_iter_attr_entry_value(iter);
    rc = std::memcmp(entry, stringEntry.data(), stringEntry.size());
    EXPECT_EQ(rc, 0);

    pldm_bios_table_iter_next(iter);
    entry = pldm_bios_table_iter_attr_entry_value(iter);
    rc = std::memcmp(entry, integerEntry.data(), integerEntry.size());
    EXPECT_EQ(rc, 0);

    pldm_bios_table_iter_next(iter);
    entry = pldm_bios_table_iter_attr_entry_value(iter);
    rc = std::memcmp(entry, enumEntry.data(), enumEntry.size());
    EXPECT_EQ(rc, 0);

    pldm_bios_table_iter_next(iter);
    EXPECT_TRUE(pldm_bios_table_iter_is_end(iter));
    pldm_bios_table_iter_free(iter);
}

TEST(AttrValTable, EnumEntryEncodeTest)
{
    std::vector<uint8_t> enumEntry{
        0, 0, /* attr handle */
        0,    /* attr type */
        2,    /* number of current value */
        0,    /* current value string handle index */
        1,    /* current value string handle index */
    };

    auto length = pldm_bios_table_attr_value_entry_encode_enum_length(2);
    EXPECT_EQ(length, enumEntry.size());
    std::vector<uint8_t> encodeEntry(length, 0);
    uint8_t handles[] = {0, 1};
    pldm_bios_table_attr_value_entry_encode_enum(
        encodeEntry.data(), encodeEntry.size(), 0, 0, 2, handles);
    EXPECT_EQ(encodeEntry, enumEntry);

    EXPECT_DEATH(
        pldm_bios_table_attr_value_entry_encode_enum(
            encodeEntry.data(), encodeEntry.size() - 1, 0, 0, 2, handles),
        "length <= entry_length");

    auto rc = pldm_bios_table_attr_value_entry_encode_enum_check(
        encodeEntry.data(), encodeEntry.size(), 0, PLDM_BIOS_ENUMERATION, 2,
        handles);
    EXPECT_EQ(rc, PLDM_SUCCESS);
    EXPECT_EQ(encodeEntry, enumEntry);
    auto entry = reinterpret_cast<struct pldm_bios_attr_val_table_entry*>(
        enumEntry.data());
    entry->attr_type = PLDM_BIOS_ENUMERATION_READ_ONLY;
    rc = pldm_bios_table_attr_value_entry_encode_enum_check(
        encodeEntry.data(), encodeEntry.size(), 0,
        PLDM_BIOS_ENUMERATION_READ_ONLY, 2, handles);
    EXPECT_EQ(rc, PLDM_SUCCESS);
    EXPECT_EQ(encodeEntry, enumEntry);
    rc = pldm_bios_table_attr_value_entry_encode_enum_check(
        encodeEntry.data(), encodeEntry.size(), 0, PLDM_BIOS_PASSWORD, 2,
        handles);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_DATA);
    rc = pldm_bios_table_attr_value_entry_encode_enum_check(
        encodeEntry.data(), encodeEntry.size() - 1, 0, PLDM_BIOS_ENUMERATION, 2,
        handles);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_LENGTH);
}

TEST(AttrValTable, EnumEntryDecodeTest)
{
    std::vector<uint8_t> enumEntry{
        0, 0, /* attr handle */
        0,    /* attr type */
        2,    /* number of current value */
        0,    /* current value string handle index */
        1,    /* current value string handle index */
    };

    auto entry = reinterpret_cast<struct pldm_bios_attr_val_table_entry*>(
        enumEntry.data());
    auto number = pldm_bios_table_attr_value_entry_enum_decode_number(entry);
    EXPECT_EQ(2, number);
}

TEST(AttrValTable, stringEntryEncodeTest)
{
    std::vector<uint8_t> stringEntry{
        0,   0,        /* attr handle */
        1,             /* attr type */
        3,   0,        /* current string length */
        'a', 'b', 'c', /* defaut value string handle index */
    };

    auto length = pldm_bios_table_attr_value_entry_encode_string_length(3);
    EXPECT_EQ(length, stringEntry.size());
    std::vector<uint8_t> encodeEntry(length, 0);
    pldm_bios_table_attr_value_entry_encode_string(
        encodeEntry.data(), encodeEntry.size(), 0, 1, 3, "abc");
    EXPECT_EQ(encodeEntry, stringEntry);

    EXPECT_DEATH(
        pldm_bios_table_attr_value_entry_encode_string(
            encodeEntry.data(), encodeEntry.size() - 1, 0, 1, 3, "abc"),
        "length <= entry_length");

    auto rc = pldm_bios_table_attr_value_entry_encode_string_check(
        encodeEntry.data(), encodeEntry.size(), 0, PLDM_BIOS_STRING, 3, "abc");
    EXPECT_EQ(rc, PLDM_SUCCESS);
    EXPECT_EQ(encodeEntry, stringEntry);
    auto entry = reinterpret_cast<struct pldm_bios_attr_val_table_entry*>(
        stringEntry.data());
    entry->attr_type = PLDM_BIOS_STRING_READ_ONLY;
    rc = pldm_bios_table_attr_value_entry_encode_string_check(
        encodeEntry.data(), encodeEntry.size(), 0, PLDM_BIOS_STRING_READ_ONLY,
        3, "abc");
    EXPECT_EQ(rc, PLDM_SUCCESS);
    EXPECT_EQ(encodeEntry, stringEntry);
    rc = pldm_bios_table_attr_value_entry_encode_string_check(
        encodeEntry.data(), encodeEntry.size(), 0, PLDM_BIOS_PASSWORD, 3,
        "abc");
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_DATA);
    rc = pldm_bios_table_attr_value_entry_encode_string_check(
        encodeEntry.data(), encodeEntry.size() - 1, 0, PLDM_BIOS_STRING, 3,
        "abc");
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_LENGTH);
}

TEST(AttrValTable, StringEntryDecodeTest)
{
    std::vector<uint8_t> stringEntry{
        0,   0,        /* attr handle */
        1,             /* attr type */
        3,   0,        /* current string length */
        'a', 'b', 'c', /* defaut value string handle index */
    };

    auto entry = reinterpret_cast<struct pldm_bios_attr_val_table_entry*>(
        stringEntry.data());
    auto length = pldm_bios_table_attr_value_entry_string_decode_length(entry);
    EXPECT_EQ(3, length);

    auto handle = pldm_bios_table_attr_value_entry_decode_handle(entry);
    EXPECT_EQ(0, handle);

    auto valueLength = pldm_bios_table_attr_value_entry_value_length(entry);
    EXPECT_EQ(5, valueLength);
    auto value = pldm_bios_table_attr_value_entry_value(entry);
    EXPECT_EQ(0, std::memcmp(value, stringEntry.data() + 3, valueLength));
}

TEST(AttrValTable, integerEntryEncodeTest)
{
    std::vector<uint8_t> integerEntry{
        0,  0,                   /* attr handle */
        3,                       /* attr type */
        10, 0, 0, 0, 0, 0, 0, 0, /* current value */
    };

    auto length = pldm_bios_table_attr_value_entry_encode_integer_length();
    EXPECT_EQ(length, integerEntry.size());
    std::vector<uint8_t> encodeEntry(length, 0);
    pldm_bios_table_attr_value_entry_encode_integer(
        encodeEntry.data(), encodeEntry.size(), 0, PLDM_BIOS_INTEGER, 10);
    EXPECT_EQ(encodeEntry, integerEntry);

    EXPECT_DEATH(pldm_bios_table_attr_value_entry_encode_integer(
                     encodeEntry.data(), encodeEntry.size() - 1, 0,
                     PLDM_BIOS_INTEGER, 10),
                 "length <= entry_length");

    auto rc = pldm_bios_table_attr_value_entry_encode_integer_check(
        encodeEntry.data(), encodeEntry.size(), 0, PLDM_BIOS_INTEGER, 10);
    EXPECT_EQ(rc, PLDM_SUCCESS);
    EXPECT_EQ(encodeEntry, integerEntry);
    auto entry = reinterpret_cast<struct pldm_bios_attr_val_table_entry*>(
        integerEntry.data());
    entry->attr_type = PLDM_BIOS_INTEGER_READ_ONLY;
    rc = pldm_bios_table_attr_value_entry_encode_integer_check(
        encodeEntry.data(), encodeEntry.size(), 0, PLDM_BIOS_INTEGER_READ_ONLY,
        10);
    EXPECT_EQ(rc, PLDM_SUCCESS);
    EXPECT_EQ(encodeEntry, integerEntry);

    rc = pldm_bios_table_attr_value_entry_encode_integer_check(
        encodeEntry.data(), encodeEntry.size(), 0, PLDM_BIOS_PASSWORD, 10);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_DATA);
    rc = pldm_bios_table_attr_value_entry_encode_integer_check(
        encodeEntry.data(), encodeEntry.size() - 1, 0,
        PLDM_BIOS_INTEGER_READ_ONLY, 10);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_LENGTH);
}

TEST(AttrValTable, IteratorTest)
{
    std::vector<uint8_t> enumEntry{
        0, 0, /* attr handle */
        0,    /* attr type */
        2,    /* number of current value */
        0,    /* current value string handle index */
        1,    /* current value string handle index */
    };
    std::vector<uint8_t> stringEntry{
        0,   0,        /* attr handle */
        1,             /* attr type */
        3,   0,        /* current string length */
        'a', 'b', 'c', /* defaut value string handle index */
    };
    std::vector<uint8_t> integerEntry{
        0,  0,                   /* attr handle */
        3,                       /* attr type */
        10, 0, 0, 0, 0, 0, 0, 0, /* current value */
    };

    Table table;
    buildTable(table, enumEntry, stringEntry, integerEntry, enumEntry);

    auto iter = pldm_bios_table_iter_create(table.data(), table.size(),
                                            PLDM_BIOS_ATTR_VAL_TABLE);
    auto entry = pldm_bios_table_iter_attr_value_entry_value(iter);

    auto p = reinterpret_cast<const uint8_t*>(entry);
    EXPECT_THAT(std::vector<uint8_t>(p, p + enumEntry.size()),
                ElementsAreArray(enumEntry));

    pldm_bios_table_iter_next(iter);
    entry = pldm_bios_table_iter_attr_value_entry_value(iter);
    p = reinterpret_cast<const uint8_t*>(entry);
    EXPECT_THAT(std::vector<uint8_t>(p, p + stringEntry.size()),
                ElementsAreArray(stringEntry));

    pldm_bios_table_iter_next(iter);
    entry = pldm_bios_table_iter_attr_value_entry_value(iter);
    p = reinterpret_cast<const uint8_t*>(entry);
    EXPECT_THAT(std::vector<uint8_t>(p, p + integerEntry.size()),
                ElementsAreArray(integerEntry));

    pldm_bios_table_iter_next(iter);
    entry = pldm_bios_table_iter_attr_value_entry_value(iter);
    p = reinterpret_cast<const uint8_t*>(entry);
    EXPECT_THAT(std::vector<uint8_t>(p, p + enumEntry.size()),
                ElementsAreArray(enumEntry));

    pldm_bios_table_iter_next(iter);
    EXPECT_TRUE(pldm_bios_table_iter_is_end(iter));

    pldm_bios_table_iter_free(iter);
}

TEST(AttrValTable, FindTest)
{
    std::vector<uint8_t> enumEntry{
        0, 0, /* attr handle */
        0,    /* attr type */
        2,    /* number of current value */
        0,    /* current value string handle index */
        1,    /* current value string handle index */
    };
    std::vector<uint8_t> stringEntry{
        1,   0,        /* attr handle */
        1,             /* attr type */
        3,   0,        /* current string length */
        'a', 'b', 'c', /* defaut value string handle index */
    };
    std::vector<uint8_t> integerEntry{
        2,  0,                   /* attr handle */
        3,                       /* attr type */
        10, 0, 0, 0, 0, 0, 0, 0, /* current value */
    };

    Table table;
    buildTable(table, enumEntry, stringEntry, integerEntry);

    auto entry = pldm_bios_table_attr_value_find_by_handle(table.data(),
                                                           table.size(), 1);
    EXPECT_NE(entry, nullptr);
    auto p = reinterpret_cast<const uint8_t*>(entry);
    EXPECT_THAT(std::vector<uint8_t>(p, p + stringEntry.size()),
                ElementsAreArray(stringEntry));

    entry = pldm_bios_table_attr_value_find_by_handle(table.data(),
                                                      table.size(), 3);
    EXPECT_EQ(entry, nullptr);

    auto firstEntry =
        reinterpret_cast<struct pldm_bios_attr_val_table_entry*>(table.data());
    firstEntry->attr_type = PLDM_BIOS_PASSWORD;
    EXPECT_DEATH(pldm_bios_table_attr_value_find_by_handle(table.data(),
                                                           table.size(), 1),
                 "entry_length != NULL");
}

TEST(StringTable, EntryEncodeTest)
{
    std::vector<uint8_t> stringEntry{
        0,   0,                            /* string handle*/
        7,   0,                            /* string length */
        'A', 'l', 'l', 'o', 'w', 'e', 'd', /* string */
    };

    const char* str = "Allowed";
    auto str_length = std::strlen(str);
    auto encodeLength = pldm_bios_table_string_entry_encode_length(str_length);
    EXPECT_EQ(encodeLength, stringEntry.size());

    std::vector<uint8_t> encodeEntry(encodeLength, 0);
    pldm_bios_table_string_entry_encode(encodeEntry.data(), encodeEntry.size(),
                                        str, str_length);
    // set string handle = 0
    encodeEntry[0] = 0;
    encodeEntry[1] = 0;

    EXPECT_EQ(stringEntry, encodeEntry);

    EXPECT_DEATH(pldm_bios_table_string_entry_encode(encodeEntry.data(),
                                                     encodeEntry.size() - 1,
                                                     str, str_length),
                 "length <= entry_length");
    auto rc = pldm_bios_table_string_entry_encode_check(
        encodeEntry.data(), encodeEntry.size() - 1, str, str_length);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_LENGTH);
}

TEST(StringTable, EntryDecodeTest)
{
    std::vector<uint8_t> stringEntry{
        4,   0,                            /* string handle*/
        7,   0,                            /* string length */
        'A', 'l', 'l', 'o', 'w', 'e', 'd', /* string */
    };
    auto entry = reinterpret_cast<struct pldm_bios_string_table_entry*>(
        stringEntry.data());
    auto handle = pldm_bios_table_string_entry_decode_handle(entry);
    EXPECT_EQ(handle, 4);
    auto strLength = pldm_bios_table_string_entry_decode_string_length(entry);
    EXPECT_EQ(strLength, 7);

    std::vector<char> buffer(strLength + 1, 0);
    auto decodedLength = pldm_bios_table_string_entry_decode_string(
        entry, buffer.data(), buffer.size());
    EXPECT_EQ(decodedLength, strLength);
    EXPECT_EQ(std::strcmp("Allowed", buffer.data()), 0);
    decodedLength =
        pldm_bios_table_string_entry_decode_string(entry, buffer.data(), 2);
    EXPECT_EQ(decodedLength, 2);
    EXPECT_EQ(std::strcmp("Al", buffer.data()), 0);

    auto rc = pldm_bios_table_string_entry_decode_string_check(
        entry, buffer.data(), buffer.size());
    EXPECT_EQ(rc, PLDM_SUCCESS);
    EXPECT_EQ(std::strcmp("Allowed", buffer.data()), 0);

    rc = pldm_bios_table_string_entry_decode_string_check(entry, buffer.data(),
                                                          buffer.size() - 1);
    EXPECT_EQ(rc, PLDM_ERROR_INVALID_LENGTH);
}

TEST(StringTable, IteratorTest)
{
    std::vector<uint8_t> stringHello{
        0,   0,                  /* string handle*/
        5,   0,                  /* string length */
        'H', 'e', 'l', 'l', 'o', /* string */
    };
    std::vector<uint8_t> stringWorld{
        1,   0,                       /* string handle*/
        6,   0,                       /* string length */
        'W', 'o', 'r', 'l', 'd', '!', /* string */
    };

    Table table;
    buildTable(table, stringHello, stringWorld);

    auto iter = pldm_bios_table_iter_create(table.data(), table.size(),
                                            PLDM_BIOS_STRING_TABLE);
    auto entry = pldm_bios_table_iter_string_entry_value(iter);
    auto rc = std::memcmp(entry, stringHello.data(), stringHello.size());
    EXPECT_EQ(rc, 0);
    pldm_bios_table_iter_next(iter);
    entry = pldm_bios_table_iter_string_entry_value(iter);
    rc = std::memcmp(entry, stringWorld.data(), stringWorld.size());
    EXPECT_EQ(rc, 0);
    pldm_bios_table_iter_next(iter);
    EXPECT_TRUE(pldm_bios_table_iter_is_end(iter));
    pldm_bios_table_iter_free(iter);
}

TEST(StringTable, FindTest)
{
    std::vector<uint8_t> stringHello{
        1,   0,                  /* string handle*/
        5,   0,                  /* string length */
        'H', 'e', 'l', 'l', 'o', /* string */
    };
    std::vector<uint8_t> stringWorld{
        2,   0,                       /* string handle*/
        6,   0,                       /* string length */
        'W', 'o', 'r', 'l', 'd', '!', /* string */
    };
    std::vector<uint8_t> stringHi{
        3,   0,   /* string handle*/
        2,   0,   /* string length */
        'H', 'i', /* string */
    };

    Table table;
    buildTable(table, stringHello, stringWorld, stringHi);

    auto entry = pldm_bios_table_string_find_by_string(table.data(),
                                                       table.size(), "World!");
    EXPECT_NE(entry, nullptr);
    auto handle = pldm_bios_table_string_entry_decode_handle(entry);
    EXPECT_EQ(handle, 2);

    entry = pldm_bios_table_string_find_by_string(table.data(), table.size(),
                                                  "Worl");
    EXPECT_EQ(entry, nullptr);

    entry =
        pldm_bios_table_string_find_by_handle(table.data(), table.size(), 3);
    EXPECT_NE(entry, nullptr);
    auto str_length = pldm_bios_table_string_entry_decode_string_length(entry);
    EXPECT_EQ(str_length, 2);
    std::vector<char> strBuf(str_length + 1, 0);
    auto rc = pldm_bios_table_string_entry_decode_string_check(
        entry, strBuf.data(), strBuf.size());
    EXPECT_EQ(rc, PLDM_SUCCESS);
    EXPECT_EQ(std::strcmp("Hi", strBuf.data()), 0);

    entry =
        pldm_bios_table_string_find_by_handle(table.data(), table.size(), 4);
    EXPECT_EQ(entry, nullptr);
}

TEST(Itearator, DeathTest)
{

    Table table(256, 0);

    /* first entry */
    auto attr_entry =
        reinterpret_cast<struct pldm_bios_attr_table_entry*>(table.data());
    auto iter = pldm_bios_table_iter_create(table.data(), table.size(),
                                            PLDM_BIOS_ATTR_TABLE);
    attr_entry->attr_type = PLDM_BIOS_PASSWORD;
    EXPECT_DEATH(pldm_bios_table_iter_next(iter), "attr_table_entry != NULL");
    pldm_bios_table_iter_free(iter);
}

TEST(PadAndChecksum, PadAndChecksum)
{
    EXPECT_EQ(4, pldm_bios_table_pad_checksum_size(0));
    EXPECT_EQ(7, pldm_bios_table_pad_checksum_size(1));
    EXPECT_EQ(6, pldm_bios_table_pad_checksum_size(2));
    EXPECT_EQ(5, pldm_bios_table_pad_checksum_size(3));
    EXPECT_EQ(4, pldm_bios_table_pad_checksum_size(4));

    // The table is borrowed from
    // https://github.com/openbmc/pldm/commit/69d3e7fb2d9935773f4fbf44326c33f3fc0a3c38
    // refer to the commit message
    Table attrValTable = {0x09, 0x00, 0x01, 0x02, 0x00, 0x65, 0x66};
    auto sizeWithoutPad = attrValTable.size();
    attrValTable.resize(sizeWithoutPad +
                        pldm_bios_table_pad_checksum_size(sizeWithoutPad));
    pldm_bios_table_append_pad_checksum(attrValTable.data(),
                                        attrValTable.size(), sizeWithoutPad);
    Table expectedTable = {0x09, 0x00, 0x01, 0x02, 0x00, 0x65,
                           0x66, 0x00, 0x6d, 0x81, 0x4a, 0xb6};
    EXPECT_EQ(attrValTable, expectedTable);
}
