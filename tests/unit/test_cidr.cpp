/**
 * test_cidr.cpp - Unit tests for CIDR parsing and matching
 *
 * Validates: Requirements 6.3, 6.4, 6.5
 */
#include <gtest/gtest.h>

extern "C" {
#include "htaccess_cidr.h"
}

/* ---- Helper: build a host-order IP from four octets ---- */
static uint32_t make_ip(uint8_t a, uint8_t b, uint8_t c, uint8_t d)
{
    return ((uint32_t)a << 24) | ((uint32_t)b << 16) |
           ((uint32_t)c << 8)  | (uint32_t)d;
}

/* ==================================================================
 *  cidr_parse — valid inputs
 * ================================================================== */

TEST(CidrParse, ClassA_Slash8)
{
    cidr_v4_t c;
    ASSERT_EQ(cidr_parse("10.0.0.0/8", &c), 0);
    EXPECT_EQ(c.network, make_ip(10, 0, 0, 0));
    EXPECT_EQ(c.mask, 0xFF000000u);
}

TEST(CidrParse, ClassB_Slash16)
{
    cidr_v4_t c;
    ASSERT_EQ(cidr_parse("172.16.0.0/16", &c), 0);
    EXPECT_EQ(c.network, make_ip(172, 16, 0, 0));
    EXPECT_EQ(c.mask, 0xFFFF0000u);
}

TEST(CidrParse, ClassC_Slash24)
{
    cidr_v4_t c;
    ASSERT_EQ(cidr_parse("192.168.1.0/24", &c), 0);
    EXPECT_EQ(c.network, make_ip(192, 168, 1, 0));
    EXPECT_EQ(c.mask, 0xFFFFFF00u);
}

TEST(CidrParse, HostAddress_Slash32)
{
    cidr_v4_t c;
    ASSERT_EQ(cidr_parse("10.20.30.40/32", &c), 0);
    EXPECT_EQ(c.network, make_ip(10, 20, 30, 40));
    EXPECT_EQ(c.mask, 0xFFFFFFFFu);
}

TEST(CidrParse, Slash0_MatchesAll)
{
    cidr_v4_t c;
    ASSERT_EQ(cidr_parse("0.0.0.0/0", &c), 0);
    EXPECT_EQ(c.network, 0u);
    EXPECT_EQ(c.mask, 0u);
}

TEST(CidrParse, PlainIP_TreatedAsSlash32)
{
    cidr_v4_t c;
    ASSERT_EQ(cidr_parse("192.168.1.100", &c), 0);
    EXPECT_EQ(c.network, make_ip(192, 168, 1, 100));
    EXPECT_EQ(c.mask, 0xFFFFFFFFu);
}

TEST(CidrParse, AllKeyword_Lowercase)
{
    cidr_v4_t c;
    ASSERT_EQ(cidr_parse("all", &c), 0);
    EXPECT_EQ(c.network, 0u);
    EXPECT_EQ(c.mask, 0u);
}

TEST(CidrParse, AllKeyword_MixedCase)
{
    cidr_v4_t c;
    ASSERT_EQ(cidr_parse("All", &c), 0);
    EXPECT_EQ(c.network, 0u);
    EXPECT_EQ(c.mask, 0u);
}

TEST(CidrParse, AllKeyword_Uppercase)
{
    cidr_v4_t c;
    ASSERT_EQ(cidr_parse("ALL", &c), 0);
    EXPECT_EQ(c.network, 0u);
    EXPECT_EQ(c.mask, 0u);
}

TEST(CidrParse, NetworkBitsMasked)
{
    /* 192.168.1.100/24 should store network as 192.168.1.0 */
    cidr_v4_t c;
    ASSERT_EQ(cidr_parse("192.168.1.100/24", &c), 0);
    EXPECT_EQ(c.network, make_ip(192, 168, 1, 0));
}

TEST(CidrParse, LeadingWhitespace)
{
    cidr_v4_t c;
    ASSERT_EQ(cidr_parse("  10.0.0.0/8", &c), 0);
    EXPECT_EQ(c.network, make_ip(10, 0, 0, 0));
}

TEST(CidrParse, TrailingWhitespace)
{
    cidr_v4_t c;
    ASSERT_EQ(cidr_parse("10.0.0.0/8  ", &c), 0);
    EXPECT_EQ(c.network, make_ip(10, 0, 0, 0));
}

/* ==================================================================
 *  cidr_parse — invalid inputs
 * ================================================================== */

TEST(CidrParse, NullString_ReturnsError)
{
    cidr_v4_t c;
    EXPECT_EQ(cidr_parse(nullptr, &c), -1);
}

TEST(CidrParse, NullOutput_ReturnsError)
{
    EXPECT_EQ(cidr_parse("10.0.0.0/8", nullptr), -1);
}

TEST(CidrParse, EmptyString_ReturnsError)
{
    cidr_v4_t c;
    EXPECT_EQ(cidr_parse("", &c), -1);
}

TEST(CidrParse, PrefixTooLarge_ReturnsError)
{
    cidr_v4_t c;
    EXPECT_EQ(cidr_parse("10.0.0.0/33", &c), -1);
}

TEST(CidrParse, OctetTooLarge_ReturnsError)
{
    cidr_v4_t c;
    EXPECT_EQ(cidr_parse("256.0.0.0/8", &c), -1);
}

TEST(CidrParse, TooFewOctets_ReturnsError)
{
    cidr_v4_t c;
    EXPECT_EQ(cidr_parse("10.0.0/8", &c), -1);
}

TEST(CidrParse, TrailingGarbage_ReturnsError)
{
    cidr_v4_t c;
    EXPECT_EQ(cidr_parse("10.0.0.0/8xyz", &c), -1);
}

TEST(CidrParse, LeadingZeroOctet_ReturnsError)
{
    cidr_v4_t c;
    EXPECT_EQ(cidr_parse("010.0.0.0/8", &c), -1);
}

TEST(CidrParse, NegativeOctet_ReturnsError)
{
    cidr_v4_t c;
    EXPECT_EQ(cidr_parse("-1.0.0.0/8", &c), -1);
}

/* ==================================================================
 *  cidr_match
 * ================================================================== */

TEST(CidrMatch, IPInRange_Slash24)
{
    cidr_v4_t c;
    cidr_parse("192.168.1.0/24", &c);
    EXPECT_EQ(cidr_match(&c, make_ip(192, 168, 1, 42)), 1);
}

TEST(CidrMatch, IPOutOfRange_Slash24)
{
    cidr_v4_t c;
    cidr_parse("192.168.1.0/24", &c);
    EXPECT_EQ(cidr_match(&c, make_ip(192, 168, 2, 1)), 0);
}

TEST(CidrMatch, ExactHost_Slash32)
{
    cidr_v4_t c;
    cidr_parse("10.20.30.40/32", &c);
    EXPECT_EQ(cidr_match(&c, make_ip(10, 20, 30, 40)), 1);
    EXPECT_EQ(cidr_match(&c, make_ip(10, 20, 30, 41)), 0);
}

TEST(CidrMatch, AllKeyword_MatchesAnyIP)
{
    cidr_v4_t c;
    cidr_parse("all", &c);
    EXPECT_EQ(cidr_match(&c, make_ip(1, 2, 3, 4)), 1);
    EXPECT_EQ(cidr_match(&c, make_ip(255, 255, 255, 255)), 1);
    EXPECT_EQ(cidr_match(&c, 0u), 1);
}

TEST(CidrMatch, Slash0_MatchesEverything)
{
    cidr_v4_t c;
    cidr_parse("0.0.0.0/0", &c);
    EXPECT_EQ(cidr_match(&c, make_ip(192, 168, 1, 1)), 1);
    EXPECT_EQ(cidr_match(&c, make_ip(10, 0, 0, 1)), 1);
}

TEST(CidrMatch, BoundaryFirst_Slash24)
{
    cidr_v4_t c;
    cidr_parse("192.168.1.0/24", &c);
    EXPECT_EQ(cidr_match(&c, make_ip(192, 168, 1, 0)), 1);
}

TEST(CidrMatch, BoundaryLast_Slash24)
{
    cidr_v4_t c;
    cidr_parse("192.168.1.0/24", &c);
    EXPECT_EQ(cidr_match(&c, make_ip(192, 168, 1, 255)), 1);
}

TEST(CidrMatch, NullCidr_Returns0)
{
    EXPECT_EQ(cidr_match(nullptr, make_ip(1, 2, 3, 4)), 0);
}

/* ==================================================================
 *  ip_parse
 * ================================================================== */

TEST(IpParse, ValidIP)
{
    uint32_t ip;
    ASSERT_EQ(ip_parse("192.168.1.100", &ip), 0);
    EXPECT_EQ(ip, make_ip(192, 168, 1, 100));
}

TEST(IpParse, ZeroIP)
{
    uint32_t ip;
    ASSERT_EQ(ip_parse("0.0.0.0", &ip), 0);
    EXPECT_EQ(ip, 0u);
}

TEST(IpParse, MaxIP)
{
    uint32_t ip;
    ASSERT_EQ(ip_parse("255.255.255.255", &ip), 0);
    EXPECT_EQ(ip, 0xFFFFFFFFu);
}

TEST(IpParse, NullString_ReturnsError)
{
    uint32_t ip;
    EXPECT_EQ(ip_parse(nullptr, &ip), -1);
}

TEST(IpParse, InvalidFormat_ReturnsError)
{
    uint32_t ip;
    EXPECT_EQ(ip_parse("not-an-ip", &ip), -1);
}

TEST(IpParse, TrailingSlash_ReturnsError)
{
    uint32_t ip;
    EXPECT_EQ(ip_parse("10.0.0.1/24", &ip), -1);
}
