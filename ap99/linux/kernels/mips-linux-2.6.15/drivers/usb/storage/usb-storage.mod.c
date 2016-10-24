#include <linux/module.h>
#include <linux/vermagic.h>
#include <linux/compiler.h>

MODULE_INFO(vermagic, VERMAGIC_STRING);

#undef unix
struct module __this_module
__attribute__((section(".gnu.linkonce.this_module"))) = {
 .name = __stringify(KBUILD_MODNAME),
 .init = init_module,
#ifdef CONFIG_MODULE_UNLOAD
 .exit = cleanup_module,
#endif
};

static const char __module_depends[]
__attribute_used__
__attribute__((section(".modinfo"))) =
"depends=usbcore";

MODULE_ALIAS("usb:v03EBp2002d0100dc*dsc*dp*ic*isc*ip*");
MODULE_ALIAS("usb:v03EEp6901d0100dc*dsc*dp*ic*isc*ip*");
MODULE_ALIAS("usb:v03EEp6901d00*dc*dsc*dp*ic*isc*ip*");
MODULE_ALIAS("usb:v03F0p0107d0200dc*dsc*dp*ic*isc*ip*");
MODULE_ALIAS("usb:v040Dp6205d0003dc*dsc*dp*ic*isc*ip*");
MODULE_ALIAS("usb:v0409p0040d*dc*dsc*dp*ic*isc*ip*");
MODULE_ALIAS("usb:v0411p001Cd0113dc*dsc*dp*ic*isc*ip*");
MODULE_ALIAS("usb:v0419pAAF6d0100dc*dsc*dp*ic*isc*ip*");
MODULE_ALIAS("usb:v0424p0FDCd0210dc*dsc*dp*ic*isc*ip*");
MODULE_ALIAS("usb:v0457p0150d0100dc*dsc*dp*ic*isc*ip*");
MODULE_ALIAS("usb:v0451p5416d0100dc*dsc*dp*ic*isc*ip*");
MODULE_ALIAS("usb:v0482p0100d0100dc*dsc*dp*ic*isc*ip*");
MODULE_ALIAS("usb:v0482p0101d0100dc*dsc*dp*ic*isc*ip*");
MODULE_ALIAS("usb:v0482p0103d0100dc*dsc*dp*ic*isc*ip*");
MODULE_ALIAS("usb:v0482p0105d0100dc*dsc*dp*ic*isc*ip*");
MODULE_ALIAS("usb:v04A4p0004d0001dc*dsc*dp*ic*isc*ip*");
MODULE_ALIAS("usb:v04B0p0405d0100dc*dsc*dp*ic*isc*ip*");
MODULE_ALIAS("usb:v04A5p3010d0100dc*dsc*dp*ic*isc*ip*");
MODULE_ALIAS("usb:v04B8p0601d0100dc*dsc*dp*ic*isc*ip*");
MODULE_ALIAS("usb:v04B8p0602d0110dc*dsc*dp*ic*isc*ip*");
MODULE_ALIAS("usb:v04CBp0100d2210dc*dsc*dp*ic*isc*ip*");
MODULE_ALIAS("usb:v04CBp0100d220*dc*dsc*dp*ic*isc*ip*");
MODULE_ALIAS("usb:v04CBp0100d2[0-1]*dc*dsc*dp*ic*isc*ip*");
MODULE_ALIAS("usb:v04CBp0100d[0-1]*dc*dsc*dp*ic*isc*ip*");
MODULE_ALIAS("usb:v04CEp0002d0074dc*dsc*dp*ic*isc*ip*");
MODULE_ALIAS("usb:v04DAp0901d0200dc*dsc*dp*ic*isc*ip*");
MODULE_ALIAS("usb:v04DAp0901d01*dc*dsc*dp*ic*isc*ip*");
MODULE_ALIAS("usb:v04DAp0D05d0000dc*dsc*dp*ic*isc*ip*");
MODULE_ALIAS("usb:v04DAp2372d*dc*dsc*dp*ic*isc*ip*");
MODULE_ALIAS("usb:v04DAp2373d*dc*dsc*dp*ic*isc*ip*");
MODULE_ALIAS("usb:v04E6p0001d0200dc*dsc*dp*ic*isc*ip*");
MODULE_ALIAS("usb:v04E6p0002d0100dc*dsc*dp*ic*isc*ip*");
MODULE_ALIAS("usb:v04E6p0006d0100dc*dsc*dp*ic*isc*ip*");
MODULE_ALIAS("usb:v04E6p0006d0205dc*dsc*dp*ic*isc*ip*");
MODULE_ALIAS("usb:v04E6p0007d0200dc*dsc*dp*ic*isc*ip*");
MODULE_ALIAS("usb:v04E6p0007d01*dc*dsc*dp*ic*isc*ip*");
MODULE_ALIAS("usb:v04E6p0009d0200dc*dsc*dp*ic*isc*ip*");
MODULE_ALIAS("usb:v04E6p000Ad0200dc*dsc*dp*ic*isc*ip*");
MODULE_ALIAS("usb:v04E6p000Bd0100dc*dsc*dp*ic*isc*ip*");
MODULE_ALIAS("usb:v04E6p000Cd0100dc*dsc*dp*ic*isc*ip*");
MODULE_ALIAS("usb:v04E6p0101d0200dc*dsc*dp*ic*isc*ip*");
MODULE_ALIAS("usb:v04FCp80C2d0100dc*dsc*dp*ic*isc*ip*");
MODULE_ALIAS("usb:v050Dp0115d0133dc*dsc*dp*ic*isc*ip*");
MODULE_ALIAS("usb:v0525pA140d0100dc*dsc*dp*ic*isc*ip*");
MODULE_ALIAS("usb:v052Bp1801d0100dc*dsc*dp*ic*isc*ip*");
MODULE_ALIAS("usb:v052Bp1804d0100dc*dsc*dp*ic*isc*ip*");
MODULE_ALIAS("usb:v052Bp1807d0100dc*dsc*dp*ic*isc*ip*");
MODULE_ALIAS("usb:v052Bp1905d0100dc*dsc*dp*ic*isc*ip*");
MODULE_ALIAS("usb:v052Bp1911d0100dc*dsc*dp*ic*isc*ip*");
MODULE_ALIAS("usb:v054Cp0010d010[6-9]dc*dsc*dp*ic*isc*ip*");
MODULE_ALIAS("usb:v054Cp0010d0450dc*dsc*dp*ic*isc*ip*");
MODULE_ALIAS("usb:v054Cp0010d01[1-9]*dc*dsc*dp*ic*isc*ip*");
MODULE_ALIAS("usb:v054Cp0010d04[0-4]*dc*dsc*dp*ic*isc*ip*");
MODULE_ALIAS("usb:v054Cp0010d0[2-3]*dc*dsc*dp*ic*isc*ip*");
MODULE_ALIAS("usb:v054Cp0010d0500dc*dsc*dp*ic*isc*ip*");
MODULE_ALIAS("usb:v054Cp0025d0100dc*dsc*dp*ic*isc*ip*");
MODULE_ALIAS("usb:v054Cp002Cd0501dc*dsc*dp*ic*isc*ip*");
MODULE_ALIAS("usb:v054Cp002Dd0100dc*dsc*dp*ic*isc*ip*");
MODULE_ALIAS("usb:v054Cp002Ed010[6-9]dc*dsc*dp*ic*isc*ip*");
MODULE_ALIAS("usb:v054Cp002Ed0310dc*dsc*dp*ic*isc*ip*");
MODULE_ALIAS("usb:v054Cp002Ed01[1-9]*dc*dsc*dp*ic*isc*ip*");
MODULE_ALIAS("usb:v054Cp002Ed030*dc*dsc*dp*ic*isc*ip*");
MODULE_ALIAS("usb:v054Cp002Ed02*dc*dsc*dp*ic*isc*ip*");
MODULE_ALIAS("usb:v054Cp002Ed0500dc*dsc*dp*ic*isc*ip*");
MODULE_ALIAS("usb:v054Cp0032d*dc*dsc*dp*ic*isc*ip*");
MODULE_ALIAS("usb:v054Cp0058d*dc*dsc*dp*ic*isc*ip*");
MODULE_ALIAS("usb:v054Cp0069d*dc*dsc*dp*ic*isc*ip*");
MODULE_ALIAS("usb:v054Cp006Dd*dc*dsc*dp*ic*isc*ip*");
MODULE_ALIAS("usb:v054Cp016Ad*dc*dsc*dp*ic*isc*ip*");
MODULE_ALIAS("usb:v054Cp0099d*dc*dsc*dp*ic*isc*ip*");
MODULE_ALIAS("usb:v057Bp0000d0[0-2]*dc*dsc*dp*ic*isc*ip*");
MODULE_ALIAS("usb:v057Bp0000d0[3-9]*dc*dsc*dp*ic*isc*ip*");
MODULE_ALIAS("usb:v057Bp0000d[1-9]*dc*dsc*dp*ic*isc*ip*");
MODULE_ALIAS("usb:v057Bp0022d*dc*dsc*dp*ic*isc*ip*");
MODULE_ALIAS("usb:v0595p4343d2210dc*dsc*dp*ic*isc*ip*");
MODULE_ALIAS("usb:v0595p4343d220*dc*dsc*dp*ic*isc*ip*");
MODULE_ALIAS("usb:v0595p4343d2[0-1]*dc*dsc*dp*ic*isc*ip*");
MODULE_ALIAS("usb:v0595p4343d[0-1]*dc*dsc*dp*ic*isc*ip*");
MODULE_ALIAS("usb:v059FpA601d0200dc*dsc*dp*ic*isc*ip*");
MODULE_ALIAS("usb:v05ABp0060d110[4-9]dc*dsc*dp*ic*isc*ip*");
MODULE_ALIAS("usb:v05ABp0060d1110dc*dsc*dp*ic*isc*ip*");
MODULE_ALIAS("usb:v05ACp1202d*dc*dsc*dp*ic*isc*ip*");
MODULE_ALIAS("usb:v05ACp1203d*dc*dsc*dp*ic*isc*ip*");
MODULE_ALIAS("usb:v05ACp1204d*dc*dsc*dp*ic*isc*ip*");
MODULE_ALIAS("usb:v05ACp1205d*dc*dsc*dp*ic*isc*ip*");
MODULE_ALIAS("usb:v05ACp120Ad*dc*dsc*dp*ic*isc*ip*");
MODULE_ALIAS("usb:v05DCpB002d011[0-3]dc*dsc*dp*ic*isc*ip*");
MODULE_ALIAS("usb:v05DCpB002d010*dc*dsc*dp*ic*isc*ip*");
MODULE_ALIAS("usb:v05DCpB002d00*dc*dsc*dp*ic*isc*ip*");
MODULE_ALIAS("usb:v05E3p0701d*dc*dsc*dp*ic*isc*ip*");
MODULE_ALIAS("usb:v05E3p0702d*dc*dsc*dp*ic*isc*ip*");
MODULE_ALIAS("usb:v0636p0003d*dc*dsc*dp*ic*isc*ip*");
MODULE_ALIAS("usb:v0644p0000d0100dc*dsc*dp*ic*isc*ip*");
MODULE_ALIAS("usb:v066Fp8000d0001dc*dsc*dp*ic*isc*ip*");
MODULE_ALIAS("usb:v067Bp2507d0100dc*dsc*dp*ic*isc*ip*");
MODULE_ALIAS("usb:v067Bp3507d0001dc*dsc*dp*ic*isc*ip*");
MODULE_ALIAS("usb:v0686p4011d0001dc*dsc*dp*ic*isc*ip*");
MODULE_ALIAS("usb:v0686p4017d0001dc*dsc*dp*ic*isc*ip*");
MODULE_ALIAS("usb:v0693p0005d0100dc*dsc*dp*ic*isc*ip*");
MODULE_ALIAS("usb:v0781p0001d0200dc*dsc*dp*ic*isc*ip*");
MODULE_ALIAS("usb:v0781p0100d0100dc*dsc*dp*ic*isc*ip*");
MODULE_ALIAS("usb:v07ABpFCCDd*dc*dsc*dp*ic*isc*ip*");
MODULE_ALIAS("usb:v07AFp0004d013[0-3]dc*dsc*dp*ic*isc*ip*");
MODULE_ALIAS("usb:v07AFp0004d01[0-2]*dc*dsc*dp*ic*isc*ip*");
MODULE_ALIAS("usb:v07AFp0005d0100dc*dsc*dp*ic*isc*ip*");
MODULE_ALIAS("usb:v07C4pA400d*dc*dsc*dp*ic*isc*ip*");
MODULE_ALIAS("usb:v07CFp1001d[1-9]*dc*dsc*dp*ic*isc*ip*");
MODULE_ALIAS("usb:v0839p000Ad0001dc*dsc*dp*ic*isc*ip*");
MODULE_ALIAS("usb:v084Dp0011d0110dc*dsc*dp*ic*isc*ip*");
MODULE_ALIAS("usb:v090Ap1001d0100dc*dsc*dp*ic*isc*ip*");
MODULE_ALIAS("usb:v090Ap1050d0100dc*dsc*dp*ic*isc*ip*");
MODULE_ALIAS("usb:v090Ap1200d*dc*dsc*dp*ic*isc*ip*");
MODULE_ALIAS("usb:v090Cp1132d*dc*dsc*dp*ic*isc*ip*");
MODULE_ALIAS("usb:v0A17p0004d1000dc*dsc*dp*ic*isc*ip*");
MODULE_ALIAS("usb:v0A17p0006d*dc*dsc*dp*ic*isc*ip*");
MODULE_ALIAS("usb:v0D96p410Ad000[1-9]dc*dsc*dp*ic*isc*ip*");
MODULE_ALIAS("usb:v0D96p410Ad00[1-9]*dc*dsc*dp*ic*isc*ip*");
MODULE_ALIAS("usb:v0D96p410Ad0[1-9]*dc*dsc*dp*ic*isc*ip*");
MODULE_ALIAS("usb:v0D96p410Ad[1-9]*dc*dsc*dp*ic*isc*ip*");
MODULE_ALIAS("usb:v0D96p5200d000[1-9]dc*dsc*dp*ic*isc*ip*");
MODULE_ALIAS("usb:v0D96p5200d0200dc*dsc*dp*ic*isc*ip*");
MODULE_ALIAS("usb:v0D96p5200d00[1-9]*dc*dsc*dp*ic*isc*ip*");
MODULE_ALIAS("usb:v0D96p5200d01*dc*dsc*dp*ic*isc*ip*");
MODULE_ALIAS("usb:v0DD8p1060d*dc*dsc*dp*ic*isc*ip*");
MODULE_ALIAS("usb:v0DDAp0001d0012dc*dsc*dp*ic*isc*ip*");
MODULE_ALIAS("usb:v0DDAp0301d0012dc*dsc*dp*ic*isc*ip*");
MODULE_ALIAS("usb:v0ED1p6660d0300dc*dsc*dp*ic*isc*ip*");
MODULE_ALIAS("usb:v0ED1p6660d0[1-2]*dc*dsc*dp*ic*isc*ip*");
MODULE_ALIAS("usb:v0EA0p2168d0110dc*dsc*dp*ic*isc*ip*");
MODULE_ALIAS("usb:v0EA0p6828d0110dc*dsc*dp*ic*isc*ip*");
MODULE_ALIAS("usb:v0FCEpD008d0000dc*dsc*dp*ic*isc*ip*");
MODULE_ALIAS("usb:v1019p0C55d*dc*dsc*dp*ic*isc*ip*");
MODULE_ALIAS("usb:v132Bp000Bd0001dc*dsc*dp*ic*isc*ip*");
MODULE_ALIAS("usb:v1370p6828d0110dc*dsc*dp*ic*isc*ip*");
MODULE_ALIAS("usb:v2735p100Bd*dc*dsc*dp*ic*isc*ip*");
MODULE_ALIAS("usb:v4146pBA01d0100dc*dsc*dp*ic*isc*ip*");
MODULE_ALIAS("usb:v*p*d*dc*dsc*dp*ic08isc01ip01*");
MODULE_ALIAS("usb:v*p*d*dc*dsc*dp*ic08isc02ip01*");
MODULE_ALIAS("usb:v*p*d*dc*dsc*dp*ic08isc03ip01*");
MODULE_ALIAS("usb:v*p*d*dc*dsc*dp*ic08isc04ip01*");
MODULE_ALIAS("usb:v*p*d*dc*dsc*dp*ic08isc05ip01*");
MODULE_ALIAS("usb:v*p*d*dc*dsc*dp*ic08isc06ip01*");
MODULE_ALIAS("usb:v*p*d*dc*dsc*dp*ic08isc01ip00*");
MODULE_ALIAS("usb:v*p*d*dc*dsc*dp*ic08isc02ip00*");
MODULE_ALIAS("usb:v*p*d*dc*dsc*dp*ic08isc03ip00*");
MODULE_ALIAS("usb:v*p*d*dc*dsc*dp*ic08isc04ip00*");
MODULE_ALIAS("usb:v*p*d*dc*dsc*dp*ic08isc05ip00*");
MODULE_ALIAS("usb:v*p*d*dc*dsc*dp*ic08isc06ip00*");
MODULE_ALIAS("usb:v*p*d*dc*dsc*dp*ic08isc01ip50*");
MODULE_ALIAS("usb:v*p*d*dc*dsc*dp*ic08isc02ip50*");
MODULE_ALIAS("usb:v*p*d*dc*dsc*dp*ic08isc03ip50*");
MODULE_ALIAS("usb:v*p*d*dc*dsc*dp*ic08isc04ip50*");
MODULE_ALIAS("usb:v*p*d*dc*dsc*dp*ic08isc05ip50*");
MODULE_ALIAS("usb:v*p*d*dc*dsc*dp*ic08isc06ip50*");
