// license:BSD-3-Clause
// copyright-holders:Couriersud

///
/// \file test_pmfp.cpp
///
/// tests for `plib::pmfp` class
///

#include "plib/ptests.h"

#include "plib/pexception.h"
#include "plib/ppmf.h"

#include <memory>
#include <cstring>
#include <utility>

///     `plib::late_pmfp<plib::pmfp<void, pstring>> a(&nld_7493::printer);`
///     // Store the a object somewhere
///
///     // After full construction ...
///
/// ```
///     auto dele = a(this);
///     dele(pstring("Hello World!"));
/// ```

class test_late_pmfp : public plib::testing::Test
{
protected:
	class late_pmfp_object
	{
	public:
		void ap(int &r) { r = ax; }
		int ax;
	};

};

PTEST_F(test_late_pmfp, late_pmfp)
{
#if !PPMF_USE_MAME_DELEGATES
	plib::late_pmfp<plib::pmfp<void (int &)>> late(&late_pmfp_object::ap);

	late_pmfp_object a;
	a.ax = 1;
	auto mfp(late(&a));
	int r(0);
	mfp(r);
	PEXPECT_TRUE(r == 1);
#endif
}

PTEST(test_compile, compile)
{
#if !PPMF_USE_MAME_DELEGATES
	plib::pmfp_base<plib::ppmf_type::PMF, void(int)> mfp_PPMF_TYPE_PMF;
	plib::pmfp_base<plib::ppmf_type::INTERNAL_ITANIUM, void(int)> mfp_PPMF_TYPE_INTERNAL_ITANIUM;
	plib::pmfp_base<plib::ppmf_type::INTERNAL_ARM, void(int)> mfp_PPMF_TYPE_INTERNAL_ARM;
	plib::pmfp_base<plib::ppmf_type::INTERNAL_MSC, void(int)> mfp_PPMF_TYPE_INTERNAL_MSC;
	PEXPECT_TRUE(mfp_PPMF_TYPE_PMF.isnull());
	PEXPECT_TRUE(mfp_PPMF_TYPE_INTERNAL_ITANIUM.isnull());
	PEXPECT_TRUE(mfp_PPMF_TYPE_INTERNAL_ARM.isnull());
	PEXPECT_TRUE(mfp_PPMF_TYPE_INTERNAL_MSC.isnull());
#endif
	// FIXME: clang on linux seems to support GNUC_PMF_CONV - test needed
	//        Need to check for clang-cl here.
#if defined(__GNUC__) && !defined(__clang__) && !defined(__NVCC__)
	plib::pmfp_base<plib::ppmf_type::GNUC_PMF_CONV, void(int)> mfp_PPMF_TYPE_GNUC_PMF_CONV;
	PEXPECT_TRUE(mfp_PPMF_TYPE_GNUC_PMF_CONV.isnull());
#else
	PEXPECT_NE("ppmf_type::GNUC_PMF_CONV not supported on this build", "");
#endif
#if defined(__EMSCRIPTEN__)
	PEXPECT_EQ(int(plib::ppmf_internal_selector::value), int(plib::ppmf_type::INTERNAL_ARM));
#endif
}

#if defined(_MSC_VER) && defined(_WIN64) && !defined(__clang__)
namespace {
	struct pmfp_padding_left { virtual ~pmfp_padding_left() = default; int left = 1; };
	struct pmfp_padding_right { virtual ~pmfp_padding_right() = default; int right = 2; };
	struct pmfp_padding_target : pmfp_padding_left, pmfp_padding_right {
		int calls = 0;
		void callback() { ++calls; }
	};
}

PTEST(test_pmfp, msvc_multiple_inheritance_padding)
{
	pmfp_padding_target target;
	using member_type = void (pmfp_padding_target::*)();
	member_type member = &pmfp_padding_target::callback;
	static_assert(sizeof(member_type) == 16);
	unsigned char bytes[sizeof(member_type)];
	std::memcpy(bytes, &member, sizeof(member));
	// These are padding bytes in MSVC's non-virtual multiple-inheritance PMF.
	std::memset(bytes + 12, 0xcc, 4);
	std::memcpy(&member, bytes, sizeof(member));
	(target.*member)();
	PEXPECT_EQ(target.calls, 1);
	plib::pmfp<void()> callback(member, &target);
	callback();
	PEXPECT_EQ(target.calls, 2);
}
#endif
