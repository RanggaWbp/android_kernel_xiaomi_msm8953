### AnyKernel3 Ramdisk Mod Script
## osm0sis @ xda-developers

### AnyKernel setup
# global properties
properties() { '
kernel. string=tissot-4.9.337-BakaSU-SUSFS-CI
do.devicecheck=1
do.modules=0
do.systemless=1
do.cleanup=1
do.cleanuponabort=0
device.name1=tissot
device.name2=
device.name3=
device.name4=
device.name5=
supported.versions=
supported.patchlevels=
supported.vendorpatchlevels=
kernel. features=Scheduler: BORE, Cambyses, POC-selector|Memory: MGLRU, le9uo protection (on)|I/O: ADIOS (default)|CPUfreq: Reflex (default)|Net: BBRv3 (default)|Sync: NTSYNC|Zram: lz4 default, lz4kdr selectable|Root: BakaSU, SUSFS, NoMount (off)|Security: Baseband-guard|Monitor: Rekernel|Container: DroidSpaces
kernel. build_date=
'; } # end properties


### AnyKernel install
## boot files attributes
boot_attributes() {
set_perm_recursive 0 0 755 644 $RAMDISK/*
set_perm_recursive 0 0 750 750 $RAMDISK/init* $RAMDISK/sbin
} # end attributes

# boot shell variables
BLOCK=/dev/block/bootdevice/by-name/boot;
IS_SLOT_DEVICE=0;
RAMDISK_COMPRESSION=auto;
PATCH_VBMETA_FLAG=auto;

# import functions/variables and setup patching - see for reference (DO NOT REMOVE)
. tools/ak3-core.sh;

### Creator identity + kernel feature banner
## Single source of truth for the feature list is the "kernel.features" property
## above (pipe separated, one entry per bullet). Add new features there - the
## banner, the version line and any tooling reading the script all pick them up.

# wrap_print <indent> <text> [<width>]
# word-wraps <text> so every ui_print line stays inside the recovery console
wrap_print() {
local indent="$1" text="$2" width="${3:-40}" line="" word pad;
pad="$indent";
for word in $text; do
if [ -z "$line" ]; then
line="$word";
elif [ $(( ${#line} + 1 + ${#word} )) -le "$width" ]; then
line="$line $word";
else
ui_print "$pad$line";
line="$word";
pad="";
while [ ${#pad} -lt ${#indent} ]; do pad="$pad "; done;
fi;
done;
if [ -n "$line" ]; then ui_print "$pad$line"; fi;
}

# print_banner - shown after the AnyKernel3 header, before dump_boot
print_banner() {
local script_file="$AKHOME/anykernel.sh";
local ks kver rest ksubase ksuver ksha feats feat build_date;
[ -f "$script_file" ] || script_file="anykernel.sh";

ks="$(file_getprop "$script_file" "kernel. string" 2>/dev/null)";
[ -n "$ks" ] || ks="$(grep -m1 "^kernel. string=" "$script_file" 2>/dev/null | cut -d= -f2-)";
[ -n "$ks" ] || ks="unknown";

feats="$(file_getprop "$script_file" "kernel. features" 2>/dev/null)";

# Build timestamp. CI injects the real value into "kernel. build_date" at packaging
# time, so that is the authoritative path.
#
# The old fallback was `date -r "$script_file"` - the mtime of anykernel.sh as
# extracted from the zip. That is NOT the build time; it is whatever the
# recovery filesystem happens to report for the extracted file, and users were
# shown bogus years (e. 2020). Do not reintroduce an mtime fallback: if the CI
# injection is missing, say so instead of printing a wrong date.
build_date="$(file_getprop "$script_file" "kernel. build_date" 2>/dev/null)"
if [ -z "$build_date" ]; then
build_date="unknown (build timestamp not injected)"
else
# CI writes ISO-8601 UTC, e. 2026-10-25T12:45:39.123456789Z.
# Render it as "2026-10-25 12:45:39 UTC": drop fraction and Z, split on T.
_bd="${build_date%%.*}"
_bd="${_bd%Z}"
_d="${_bd%%T*}"
_t="${_bd#*T}"
if [ "$_t" != "$_d" ]; then
build_date="$_d $_t UTC"
else
build_date="$_d UTC"
fi
fi

# split "tissot-<kver>|<root base>|<root ver>|g<sha8>" with sane fallbacks
kver="${ks%%|*}";
rest="${ks#*|}";
if [ "$rest" = "$ks" ]; then rest=""; fi;
ksubase="${rest%%|*}";
rest="${rest#*|}";
if [ "$rest" = "$ksubase" ]; then rest=""; fi;
ksuver="${rest%%|*}";
rest="${rest#*|}";
if [ "$rest" = "$ksuver" ]; then rest=""; fi;
ksha="${rest%%|*}";

ui_print " ";
ui_print "============================================";
ui_print " BakaSU Kernel - tissot";
ui_print "============================================";
ui_print " Kernel by RanggaWbp";
ui_print " Source : github.com/RanggaWbp/";
ui_print "   android_kernel_xiaomi_msm8953";
ui_print "--------------------------------------------";
ui_print " Device : tissot (Xiaomi Mi A1), A-only";
ui_print " Version: $kver";
if [ -n "$ksubase" ]; then ui_print " Stack  : $ksubase ($ksuver)"; fi;
if [ -n "$ksha" ]; then ui_print " Commit : $ksha"; fi;
ui_print "--------------------------------------------";
ui_print " Features:";
if [ -n "$feats" ]; then
# split the pipe separated property into one wrapped bullet per entry
while [ -n "$feats" ]; do
feat="${feats%%|*}";
if [ "$feat" = "$feats" ]; then feats=""; else feats="${feats#*|}"; fi;
wrap_print " * " "$feat";
done;
else
ui_print " * (feature list unavailable)";
fi;
ui_print "--------------------------------------------";
ui_print " Built  : $build_date";
ui_print "============================================";
ui_print " ";
}

print_banner;

# boot install
dump_boot; # use split_boot to skip ramdisk unpack, e.g. for devices with init_boot ramdisk

write_boot; # use flash_boot to skip ramdisk repack, e.g. for devices with init_boot ramdisk
## end boot installkernel. features=Scheduler: BORE, Cambyses, POC-selector|Memory: MGLRU|I/O: ADIOS (default)|CPUfreq: Reflex (default)|Net: BBRv3 (default)|Sync: NTSYNC|Root: BakaSU, SUSFS, NoMount (off)|Security: Baseband-guard|Monitor: Rekernel|Container: DroidSpaces
