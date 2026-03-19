#!/usr/bin/env bash

# download the SPEC CPU 2017 benchmark
scp ./cpu2017.iso truejit:
# or
rsync -avzh --progress ~/Documents/cpu2017.iso truejit:/tmp
# or
cp /proj/truejit-PG0/cpu2017.iso ~

# mount the iso file
rm -rf /tmp/spec /tmp/spec_iso
mkdir -p /tmp/spec_iso
sudo mount -o loop /tmp/cpu2017.iso /tmp/spec_iso

# it's read-only, so we need to copy it to a writable location
mkdir -p /tmp/spec
cp -r /tmp/spec_iso/* /tmp/spec
sudo chmod -R 755 /tmp/spec
sudo chown -R $(whoami) /tmp/spec
cd ~ && sudo umount /tmp/spec_iso
rm -rf /tmp/spec_iso

# install the SPEC CPU 2017 benchmark
cd /tmp/spec
yes | ./install.sh
source shrc

# change the default configuration # TODO: create a file and save it in evaluation directory
cp ./config/Example-gcc-linux-x86.cfg ./config/truejit.cfg
nano ./config/truejit.cfg

# change the config
# %define label truejit
# %define gcc_dir /usr
#   OPTIMIZE       = -g -O3 -march=native -fno-unsafe-math-optimizations  -fno-tree-loop-vectorize
sed -i 's/%define label.*/%define label truejit/g' ./config/truejit.cfg
sed -i 's/%   define  gcc_dir.*/%define gcc_dir \/usr/g' ./config/truejit.cfg
sed -i 's/   OPTIMIZE       =.*/   OPTIMIZE       = -g -O3 -fgnu89-inline -march=native /g' ./config/truejit.cfg

# check changes
diff ./config/Example-gcc-linux-x86.cfg ./config/truejit.cfg


##########################
# notes on flags
# remove:
# -fno-tree-loop-vectorize
# -marc=native
# -m64

# add:
# -D_WASI_EMULATED_SIGNAL -lwasi-emulated-signal
# -D_WASI_EMULATED_PROCESS_CLOCKS -lwasi-emulated-process-clocks
# -D_WASI_EMULATED_MMAN -lwasi-emulated-mman
# -D_WASI_EMULATED_GETPID -lwasi-emulated-getpid

# e.g.: -D_WASI_EMULATED_SIGNAL -lwasi-emulated-signal -D_WASI_EMULATED_PROCESS_CLOCKS -lwasi-emulated-process-clocks -D_WASI_EMULATED_MMAN -lwasi-emulated-mman -D_WASI_EMULATED_GETPID -lwasi-emulated-getpid
##########################




# 502.gcc_r
runcpu --config=truejit --tune=base --action=build 502.gcc_r
cd /tmp/spec/benchspec/CPU/502.gcc_r/build/build_base_truejit-m64.0000
sed -i 's/#define HAVE_SYS_WAIT_H 1//g' ./auto-host.h
sed -i 's/#define HAVE_SYS_WAIT_H 1//g' ./libib_config.h
sed -i 's/#define HAVE_DECL_GETRLIMIT 1//g' ./auto-host.h
sed -i 's/#define HAVE_GETRLIMIT 1//g' ./auto-host.h
sed -i 's/#define HAVE_SETRLIMIT 1//g' ./auto-host.h
/opt/wasi-sdk/bin/clang \
  -z stack-size=$((1 << 20)) \
  -m32 \
  -std=c99 -DSPEC -DNDEBUG -I. -I./include -I./spec_qsort -DSPEC_502 -DSPEC_AUTO_SUPPRESS_OPENMP -DIN_GCC -DHAVE_CONFIG_H -g -O3 -fgnu89-inline -fno-strict-aliasing -DSPEC_LP64 \
  -D_WASI_EMULATED_SIGNAL -lwasi-emulated-signal -D_WASI_EMULATED_PROCESS_CLOCKS -lwasi-emulated-process-clocks -D_WASI_EMULATED_MMAN -lwasi-emulated-mman -D_WASI_EMULATED_GETPID -lwasi-emulated-getpid \
  cc1-checksum.c alloca.c main.c alloc-pool.c argv.c auto-inc-dec.c attribs.c c-errors.c bid2dpd_dpd2bid.c c-lang.c c-pragma.c c-lex.c bt-load.c c-convert.c bb-reorder.c c-aux-info.c alias.c bitmap.c c-semantics.c c-objc-common.c c-dump.c c-opts.c c-ppoutput.c c-gimplify.c c-omp.c c-format.c c-pretty-print.c cfg.c caller-save.c cfganal.c calls.c cfgbuild.c c-typeck.c c-decl.c cfghooks.c cfgcleanup.c cfglayout.c cfgloopanal.c cgraphbuild.c c-parser.c cfgloop.c cgraph.c c-common.c concat.c cfgloopmanip.c cfgrtl.c combine-stack-adj.c cgraphunit.c cp-demint.c cfgexpand.c cpp_errors.c convert.c coverage.c cpp_directives.c cpp_lex.c cpp_expr.c cpp_line-map.c cpp_charset.c cpp_symtab.c cpp_identifiers.c cpp_macro.c cpp_mkdeps.c crc32.c cpp_directives-only.c cpp_traditional.c cpp_pch.c dbgcnt.c cplus-dem.c debug.c dce.c decContext.c cselib.c ddg.c cp-demangle.c decimal128.c decimal32.c df-byte-scan.c decimal64.c dbxout.c dfp.c diagnostic.c df-core.c dojump.c domwalk.c double-int.c combine.c dwarf2asm.c dyn-string.c dominance.c df-problems.c et-forest.c cse.c ebitmap.c df-scan.c dse.c fdmatch.c explow.c filename_cmp.c fibheap.c except.c fixed-value.c floatformat.c decNumber.c fopen_unlocked.c final.c expmed.c fwprop.c getopt1.c getopt.c getpwd.c emit-rtl.c getruntime.c genrtl.c ggc-common.c ggc-page.c gimple-iterator.c gimple-low.c function.c gcse.c graph.c expr.c graphds.c graphite.c graphite-blocking.c graphite-clast-to-gimple.c graphite-dependences.c graphite-interchange.c graphite-ppl.c gimple-pretty-print.c graphite-poly.c graphite-scop-detection.c graphite-sese-to-poly.c hex.c gimple.c host-ieee128.c hooks.c host-ieee32.c host-ieee64.c hashtab.c host-linux.c i386-c.c incpath.c init-regs.c gimplify.c ifcvt.c haifa-sched.c dwarf2out.c insn-automata.c insn-modes.c insn-extract.c gtype-desc.c insn-peep.c insn-opinit.c integrate.c insn-output.c insn-preds.c ipa-cp.c fold-const.c ipa-inline.c ipa-prop.c ipa-pure-const.c ipa-utils.c ipa-reference.c ipa.c ipa-type-escape.c ipa-struct-reorg.c ira.c ira-build.c ira-conflicts.c ira-costs.c i386.c jump.c ira-emit.c ira-lives.c lambda-trans.c lbasename.c langhooks.c lists.c loop-doloop.c lcm.c lambda-mat.c loop-init.c ira-color.c loop-unswitch.c loop-invariant.c lrealpath.c loop-iv.c lambda-code.c loop-unroll.c lower-subreg.c insn-emit.c lto-section-in.c lto-cgraph.c lto-section-out.c lto-opts.c lto-symtab.c lto-wpa-fixup.c make-temp-file.c make-relative-prefix.c partition.c lto-streamer.c mkstemps.c md5.c lto-streamer-out.c lto-streamer-in.c obstack.c objalloc.c mode-switching.c mcf.c matrix-reorg.c opts-common.c modulo-sched.c options.c params.c physmem.c plugin.c pointer-set.c passes.c opts.c postreload-gcse.c postreload.c pretty-print.c optabs.c print-rtl.c omp-low.c predict.c omega.c print-tree.c profile.c regcprop.c reginfo.c regmove.c regstat.c regrename.c recog.c reorg.c rtl-error.c reg-stack.c rtl.c resource.c rtlhooks.c safe-ctype.c regex.c sched-ebb.c sbitmap.c sched-vis.c sdbout.c sched-deps.c sched-rgn.c sel-sched-dump.c reload1.c reload.c sha1.c sort.c spaces.c sparseset.c rtlanal.c splay-tree.c sese.c sreal.c stack-ptr-mod.c statistics.c store-motion.c stor-layout.c stringpool.c sel-sched-ir.c strsignal.c stub-objc.c timevar.c targhooks.c tracer.c simplify-rtx.c stmt.c tree-affine.c tree-call-cdce.c sel-sched.c tree-cfgcleanup.c tree-dfa.c tree-chrec.c tree-dump.c tree-complex.c tree-if-conv.c tree-iterator.c tree-eh.c tree-data-ref.c tree-loop-linear.c tree-loop-distribution.c tree-cfg.c tree-mudflap.c tree-nrv.c tree-inline.c tree-optimize.c tree-nested.c tree-object-size.c tree-phinodes.c tree-into-ssa.c tree-outof-ssa.c tree-profile.c tree-parloops.c tree-switch-conversion.c tree-pretty-print.c tree-ssa-address.c tree-scalar-evolution.c tree-predcom.c insn-recog.c tree-ssa-alias.c tree-ssa-copyrename.c tree-ssa-copy.c tree-sra.c tree-ssa-dse.c tree-ssa-coalesce.c tree-ssa-loop-ch.c tree-ssa-ifcombine.c tree-ssa-dce.c tree-ssa-live.c tree-ssa-ccp.c tree-ssa-dom.c tree-ssa-loop-ivcanon.c tree-ssa-forwprop.c tree-ssa-loop-unswitch.c tree-ssa-loop-manip.c tree-ssa-loop.c tree-ssa-loop-im.c tree-ssa-loop-prefetch.c tree-ssa-phiprop.c tree-ssa-operands.c tree-ssa-loop-niter.c tree-ssa-math-opts.c tree-ssa-phiopt.c tree-ssa-sink.c tree-ssa-propagate.c tree-ssa-loop-ivopts.c tree-ssa-ter.c tree-ssa-threadedge.c tree-ssa-reassoc.c tree-ssa-threadupdate.c tree-ssa-uncprop.c tree-ssanames.c tree-ssa.c tree-tailcall.c tree-stdarg.c tree-vect-generic.c tree-ssa-sccvn.c tree-ssa-pre.c tree-vect-patterns.c tree-vectorizer.c tree-vect-loop-manip.c tree-ssa-structalias.c unlink-if-ordinary.c tree-vect-data-refs.c tree-vect-slp.c varpool.c value-prof.c varray.c tree-vect-loop.c tree-vect-stmts.c vmsdbgout.c xatexit.c vec.c xexit.c xmalloc.c xcoffout.c xmemdup.c xstrdup.c xstrerror.c xstrndup.c web.c c-pch.c cpp_init.c cppdefault.c c-cppbuiltin.c intl.c cpp_files.c strerror.c prefix.c version.c vasprintf.c toplev.c tree-vrp.c varasm.c var-tracking.c spec_qsort/spec_qsort.c tree.c real.c builtins.c mini-gmp.c insn-attrtab.c \
  -o gcc.wasm

WASMTIME_BACKTRACE_DETAILS=1 wasmtime ./gcc.wasm --version
#Caused by:
#    0: failed to invoke command default
#    1: error while executing at wasm backtrace:
#           0: 0x17077f - init_ggc
#                           at /tmp/spec/benchspec/CPU/502.gcc_r/build/build_base_truejit-m64.0000/ggc-page.c:1585:50
#           1: 0x5be51d - general_init
#                           at /tmp/spec/benchspec/CPU/502.gcc_r/build/build_base_truejit-m64.0000/toplev.c:1726:3
#                       - toplev_main
#                           at /tmp/spec/benchspec/CPU/502.gcc_r/build/build_base_truejit-m64.0000/toplev.c:2444:3
#           2:   0x4fd7 - __main_argc_argv
#                           at /tmp/spec/benchspec/CPU/502.gcc_r/build/build_base_truejit-m64.0000/main.c:35:10
#           3: 0x6eb122 - gcc.wasm!__main_void
#           4:   0x4fb0 - gcc.wasm!_start
#    2: wasm trap: integer divide by zero

runcpu --config=truejit --tune=base --action=build 505.mcf_r

sudo apt-get install -y gfortran
runcpu --config=truejit --tune=base --action=build 507.cactuBSSN_r
sed -i 's/#define HAVE_NETDB_H 1//g' ./include/cctk_Config.h
/opt/wasi-sdk/bin/clang \
  -z stack-size=$((1 << 20)) \
  -m32 \
  -D_WASI_EMULATED_SIGNAL -lwasi-emulated-signal -D_WASI_EMULATED_PROCESS_CLOCKS -lwasi-emulated-process-clocks -D_WASI_EMULATED_MMAN -lwasi-emulated-mman -D_WASI_EMULATED_GETPID -lwasi-emulated-getpid \
  -std=c99 -DSPEC -DNDEBUG -DSPEC_AUTO_SUPPRESS_OPENMP -Iinclude -DCCODE -g -O3 -fgnu89-inline -DSPEC_LP64 \
  gnu/getopt1.c gnu/getopt.c ADMBase/cctk_Bindings/cctk_ThornBindings.c ADMCoupling/cctk_Bindings/cctk_ThornBindings.c ADMMacros/cctk_Bindings/cctk_ThornBindings.c ADMMacros/SetLocalSpatialOrder.c ADMBase/ParamCheck.c Boundary/Check.c Boundary/NoneBoundary.c ADMBase/InitSymBound.c ADMBase/Static.c Boundary/Register.c Boundary/Boundary.c Boundary/FlatBoundary.c Boundary/cctk_Bindings/cctk_ThornBindings.c Cactus/IO/FortranBindingsIO.c Boundary/CopyBoundary.c Cactus/IO/OverloadIO.c ADMBase/Initialisation.c Cactus/cctk_Bindings/cctk_ThornBindings.c Cactus/IO/IOMethods.c Cactus/comm/CactusSync.c Cactus/comm/Interp.c Boundary/StaticBoundary.c Cactus/comm/CactusDefaultComm.c Cactus/main/Banner.c Cactus/comm/OverloadComm.c Cactus/main/CactusDefaultEvolve.c Cactus/main/CactusDefaultInitialise.c Cactus/main/CactusDefaultMainLoopIndex.c Cactus/main/CactusDefaultShutdown.c Cactus/main/CallStartupFunctions.c Cactus/comm/Reduction.c Cactus/main/ActiveThorns.c Cactus/main/Complex.c Cactus/main/ConfigData.c Cactus/main/DebugDefines.c Cactus/main/DefaultTimers.c Cactus/main/CommandLine.c Cactus/main/FortranWrappers.c Cactus/main/Dummies.c Cactus/main/GHExtensions.c Cactus/main/InitialiseCactus.c Boundary/RobinBoundary.c Cactus/main/Coord.c Cactus/main/MainUtils.c Cactus/main/InitialiseDataStructures.c Cactus/main/OverloadMain.c Cactus/main/Names.c Cactus/main/ProcessEnvironment.c Cactus/main/ProcessCommandLine.c Cactus/main/ProcessParameterDatabase.c Cactus/main/SetParams.c Cactus/main/SetupCache.c Boundary/RadiationBoundary.c Cactus/main/ShutdownCactus.c Cactus/main/Subsystems.c Cactus/main/Termination.c Cactus/main/Traverse.c Cactus/main/Groups.c Cactus/main/GroupsOnGH.c Cactus/main/WarnLevel.c Boundary/ScalarBoundary.c Cactus/main/Parameters.c Cactus/main/ScheduleInterface.c gnu/regex.c Cactus/schedule/ScheduleSorter.c Cactus/schedule/ScheduleCreater.c Cactus/util/Cache.c Cactus/schedule/ScheduleTraverse.c Cactus/util/File.c Cactus/util/Malloc.c Cactus/util/Hash.c Cactus/util/Math.c Cactus/util/Network.c Cactus/util/CactusTimers.c Cactus/util/RegisterKeyedFunction.c Cactus/util/SKBinTree.c Cactus/util/ParseFile.c Cactus/util/Expression.c Cactus/util/Misc.c Cactus/util/StoreKeyedData.c Cactus/util/StoreHandledData.c Cactus/util/StoreNamedData.c Cactus/util/StringList.c Cactus/util/Time.c Cactus/util/String.c CactusBindings/Functions/ADMBase_Functions.c CactusBindings/Functions/Boundary_Functions.c CactusBindings/Functions/CartGrid3D_Functions.c CactusBindings/Functions/CoordBase_Functions.c CactusBindings/Functions/GaugeWave_Functions.c CactusBindings/Functions/GenericFD_Functions.c CactusBindings/Functions/IOASCII_Functions.c CactusBindings/Functions/IOBasic_Functions.c CactusBindings/Functions/IOUtil_Functions.c CactusBindings/Functions/ML_BSSN_Functions.c CactusBindings/Functions/ML_BSSN_Helper_Functions.c CactusBindings/Functions/IsFunctionAliased.c CactusBindings/Functions/MoL_Functions.c CactusBindings/Functions/PUGHInterp_Functions.c Cactus/util/snprintf.c CactusBindings/Functions/PUGHSlab_Functions.c CactusBindings/Functions/RegisterThornFunctions.c CactusBindings/Functions/StaticConformal_Functions.c CactusBindings/Implementations/ImplementationBindings.c CactusBindings/Parameters/ADMCoupling_Parameters.c CactusBindings/Functions/SymBase_Functions.c CactusBindings/Parameters/ADMMacros_Parameters.c CactusBindings/Parameters/ADMBase_Parameters.c CactusBindings/Parameters/BindingsParameters.c CactusBindings/Parameters/Boundary_Parameters.c CactusBindings/Parameters/Cactus_Parameters.c CactusBindings/Parameters/CartGrid3D_Parameters.c CactusBindings/Parameters/CoordGauge_Parameters.c CactusBindings/Parameters/Fortran_Parameters.c CactusBindings/Parameters/CoordBase_Parameters.c CactusBindings/Parameters/Global.c CactusBindings/Parameters/GenericFD_Parameters.c CactusBindings/Parameters/GaugeWave_Parameters.c CactusBindings/Parameters/IOBasic_Parameters.c CactusBindings/Parameters/IOASCII_Parameters.c CactusBindings/Parameters/LocalReduce_Parameters.c CactusBindings/Parameters/ML_BSSN_Helper_Parameters.c CactusBindings/Parameters/PUGHInterp_Parameters.c CactusBindings/Parameters/IOUtil_Parameters.c CactusBindings/Parameters/MoL_Parameters.c CactusBindings/Parameters/PUGHReduce_Parameters.c CactusBindings/Parameters/PUGHSlab_Parameters.c CactusBindings/Parameters/SymBase_Parameters.c CactusBindings/Parameters/StaticConformal_Parameters.c CactusBindings/Parameters/TmunuBase_Parameters.c CactusBindings/Parameters/Time_Parameters.c CactusBindings/Parameters/PUGH_Parameters.c CactusBindings/Schedule/BindingsSchedule.c CactusBindings/Schedule/BindingsParameterRecovery.c CactusBindings/Schedule/ScheduleADMCoupling.c CactusBindings/Schedule/ScheduleADMMacros.c CactusBindings/Schedule/ScheduleADMBase.c CactusBindings/Schedule/ScheduleBoundary.c CactusBindings/Schedule/ScheduleCactus.c CactusBindings/Schedule/ScheduleCoordBase.c CactusBindings/Schedule/ScheduleFortran.c CactusBindings/Schedule/ScheduleCoordGauge.c CactusBindings/Schedule/ScheduleCartGrid3D.c CactusBindings/Parameters/ML_BSSN_Parameters.c CactusBindings/Schedule/ScheduleGenericFD.c CactusBindings/Schedule/ScheduleGaugeWave.c CactusBindings/Schedule/ScheduleIOBasic.c CactusBindings/Schedule/ScheduleIOASCII.c CactusBindings/Schedule/ScheduleIOUtil.c CactusBindings/Schedule/ScheduleLocalReduce.c CactusBindings/Schedule/SchedulePUGHInterp.c CactusBindings/Schedule/SchedulePUGH.c CactusBindings/Functions/AliasedFunctions.c CactusBindings/Schedule/SchedulePUGHReduce.c CactusBindings/Schedule/SchedulePUGHSlab.c CactusBindings/Schedule/ScheduleML_BSSN_Helper.c CactusBindings/Schedule/ScheduleStaticConformal.c CactusBindings/Schedule/ScheduleSymBase.c CactusBindings/Schedule/ScheduleTime.c CactusBindings/Variables/BindingsVariables.c CactusBindings/Schedule/ScheduleTmunuBase.c CactusBindings/Variables/ADMMacros.c CactusBindings/Variables/ADMCoupling.c CactusBindings/Variables/Cactus.c CactusBindings/Variables/Boundary.c CactusBindings/Schedule/ScheduleMoL.c CactusBindings/Variables/CoordBase.c CactusBindings/Variables/CartGrid3D.c CactusBindings/Variables/ADMBase.c CactusBindings/Variables/Fortran.c CactusBindings/Variables/CoordGauge.c CactusBindings/Variables/IOASCII.c CactusBindings/Variables/IOUtil.c CactusBindings/Variables/IOBasic.c CactusBindings/Variables/LocalReduce.c CactusBindings/Schedule/ScheduleML_BSSN.c CactusBindings/Variables/GenericFD.c CactusBindings/Variables/PUGH.c CactusBindings/Variables/PUGHReduce.c CactusBindings/Variables/PUGHInterp.c CactusBindings/Variables/GaugeWave.c CactusBindings/Variables/PUGHSlab.c CactusBindings/Variables/MoL.c CactusBindings/Variables/SymBase.c CactusBindings/Variables/Time.c CactusBindings/Variables/StaticConformal.c CartGrid3D/DecodeSymParameters.c CartGrid3D/GetSymmetry.c CartGrid3D/ParamCheck.c CartGrid3D/RegisterSymmetries.c CartGrid3D/SetSymmetry.c CartGrid3D/Startup.c CactusBindings/Variables/TmunuBase.c CartGrid3D/cctk_Bindings/cctk_ThornBindings.c CactusBindings/Variables/ML_BSSN_Helper.c CartGrid3D/CartGrid3D.c CoordBase/cctk_Bindings/cctk_ThornBindings.c CoordBase/GHExtension.c CoordGauge/cctk_Bindings/cctk_ThornBindings.c CoordBase/CoordBase.c CactusBindings/Variables/ML_BSSN.c CoordGauge/Slicing.c CoordBase/Domain.c Fortran/cctk_Bindings/cctk_ThornBindings.c Cactus/util/Table.c GaugeWave/cctk_Bindings/cctk_ThornBindings.c GenericFD/cctk_Bindings/cctk_ThornBindings.c IOASCII/ChooseOutput.c IOASCII/Output2D.c IOASCII/Output1D.c IOASCII/Output3D.c IOASCII/Startup.c IOASCII/cctk_Bindings/cctk_ThornBindings.c GenericFD/GenericFD.c IOBasic/OutputInfo.c IOBasic/OutputScalar.c IOBasic/Startup.c IOASCII/Write3D.c IOBasic/cctk_Bindings/cctk_ThornBindings.c IOASCII/Write2D.c IOBasic/WriteInfo.c IOUtil/AdvertisedFiles.c IOBasic/WriteScalar.c IOASCII/Write1D.c IOUtil/cctk_Bindings/cctk_ThornBindings.c IOUtil/Startup.c IOUtil/CheckpointRecovery.c IOUtil/Utils.c CartGrid3D/Symmetry.c LocalReduce/CountFunctions.c LocalReduce/MinFunctions.c LocalReduce/Reduction.c LocalReduce/MaxFunctions.c LocalReduce/ReductionAvg.c LocalReduce/ReductionCount.c LocalReduce/ReductionMax.c LocalReduce/AvgFunctions.c LocalReduce/ReductionMin.c LocalReduce/ReductionNorm1.c LocalReduce/ReductionNorm2.c LocalReduce/ReductionNorm3.c LocalReduce/ReductionNormInf.c LocalReduce/ReductionNorm4.c LocalReduce/Norm2Functions.c LocalReduce/Startup.c LocalReduce/ReductionSum.c LocalReduce/cctk_Bindings/cctk_ThornBindings.c LocalReduce/Norm1Functions.c LocalReduce/Norm3Functions.c LocalReduce/Norm4Functions.c ML_BSSN/cctk_Bindings/cctk_ThornBindings.c ML_BSSN_Helper/ExtrapolateGammas.c ML_BSSN_Helper/RegisterSlicing.c ML_BSSN_Helper/NewRad.c LocalReduce/NormInfFunctions.c ML_BSSN_Helper/RegisterConstrained.c ML_BSSN_Helper/cctk_Bindings/cctk_ThornBindings.c ML_BSSN_Helper/SetGroupTags.c ML_BSSN_Helper/SelectBCsADMBase.c MoL/Counter.c MoL/Euler.c MoL/AB.c MoL/ChangeType.c MoL/IndexArrays.c MoL/ICN.c MoL/GenericRK.c MoL/ParamCheck.c MoL/RHSNaNCheck.c MoL/RK2.c MoL/Operators.c MoL/InitialCopy.c MoL/RK3.c MoL/RK2-MR-2_1.c MoL/RK4.c MoL/RK4-MR-2_1.c MoL/RK4-RK2.c MoL/RK45.c MoL/RKCoefficients.c MoL/SandR.c MoL/RK87.c MoL/Startup.c MoL/RK65.c MoL/cctk_Bindings/cctk_ThornBindings.c PUGH/Evolve.c MoL/SetTime.c PUGH/Comm.c PUGH/FinishReceiveGA.c PUGH/Overloadables.c MoL/StepSize.c PUGH/LoadAware.c PUGH/PostReceiveGA.c PUGH/P2LMappings.c PUGH/PostSendGA.c MoL/Registration.c PUGH/GHExtension.c PUGH/Registration.c PUGH/PughUtils.c PUGH/SetupGroup.c PUGH/Startup.c PUGH/SetupPGH.c PUGH/cctk_Bindings/cctk_ThornBindings.c PUGH/Topology.c PUGHInterp/Startup.c PUGHInterp/cctk_Bindings/cctk_ThornBindings.c PUGHInterp/InterpGridArrays.c PUGHReduce/ReduceArraysGlobally.c PUGH/Storage.c PUGHReduce/ReduceGA.c PUGHReduce/ReductionCount.c PUGH/SetupPGV.c PUGHReduce/Reduction.c PUGHReduce/ReductionAvg.c PUGHReduce/ReductionMin.c PUGHReduce/ReductionNorm2.c PUGHReduce/ReductionMax.c PUGHReduce/ReductionNorm1.c PUGHReduce/Startup.c PUGHReduce/cctk_Bindings/cctk_ThornBindings.c PUGHSlab/DatatypeConversion.c PUGHSlab/GetHyperslab.c PUGHReduce/ReductionNorm3.c PUGHSlab/cctk_Bindings/cctk_ThornBindings.c StaticConformal/Initialise.c StaticConformal/ConfPhys.c StaticConformal/cctk_Bindings/cctk_ThornBindings.c SymBase/Check.c LocalReduce/SumFunctions.c SymBase/Handles.c SymBase/Faces.c SymBase/Interpolation.c SymBase/Startup.c SymBase/Statistics.c PUGHReduce/ReductionNormInf.c SymBase/cctk_Bindings/cctk_ThornBindings.c PUGHSlab/Mapping.c PUGHReduce/ReductionSum.c Time/Given.c SymBase/Table.c PUGHReduce/ReductionNorm4.c Time/cctk_Bindings/cctk_ThornBindings.c Time/Initialise.c Time/Courant.c Time/Simple.c TmunuBase/cctk_Bindings/cctk_ThornBindings.c datestamp.c \
  -o cactusBSSN.wasm
WASMTIME_BACKTRACE_DETAILS=1 wasmtime --dir=$(pwd)/data/test/input/ ./cactusBSSN.wasm $(pwd)/data/test/input/spec_test.par
#Error: failed to run main module `./cactusBSSN.wasm`
#
#Caused by:
#    0: failed to invoke command default
#    1: error while executing at wasm backtrace:
#           0:    0x228 - cactusBSSN.wasm!undefined_weak:main
#           1:   0x23e4 - cactusBSSN.wasm!__main_void
#           2:    0x25c - cactusBSSN.wasm!_start
#    2: wasm trap: wasm `unreachable` instruction executed

runcpu --config=truejit --tune=base --action=build 511.povray_r
cd /tmp/spec/benchspec/CPU/511.povray_r/build/build_base_truejit-m64.0000
/opt/wasi-sdk/bin/clang \
  -z stack-size=$((1 << 20)) \
  -m32 \
  -D_WASI_EMULATED_SIGNAL -lwasi-emulated-signal -D_WASI_EMULATED_PROCESS_CLOCKS -lwasi-emulated-process-clocks -D_WASI_EMULATED_MMAN -lwasi-emulated-mman -D_WASI_EMULATED_GETPID -lwasi-emulated-getpid \
  -DSPEC -DNDEBUG -Ifrontend -Ibase -I. -Ispec_qsort -DSPEC_AUTO_SUPPRESS_OPENMP -g -O3 -fgnu89-inline -DSPEC_LP64  image_validator/ImageValidator.c \
  -o imagevalidate_511.wasm

/opt/wasi-sdk/bin/clang++ \
  -z stack-size=$((1 << 20)) \
  -m32 \
  -D_WASI_EMULATED_SIGNAL -lwasi-emulated-signal -D_WASI_EMULATED_PROCESS_CLOCKS -lwasi-emulated-process-clocks -D_WASI_EMULATED_MMAN -lwasi-emulated-mman -D_WASI_EMULATED_GETPID -lwasi-emulated-getpid \
  -DSPEC -DNDEBUG -Ifrontend -Ibase -I. -Ispec_qsort -DSPEC_AUTO_SUPPRESS_OPENMP  -g -O3 -DSPEC_LP64 \
  -mllvm -wasm-enable-sjlj \
  -Wno-register \
  spec_qsort/spec_qsort.c \
  benchmark.cpp boxes.cpp bsphere.cpp bcyl.cpp camera.cpp atmosph.cpp bbox.cpp chi2.cpp colutils.cpp colour.cpp cones.cpp discs.cpp bezier.cpp blob.cpp csg.cpp fnsyntax.cpp fncode.cpp fractal.cpp function.cpp histogra.cpp fpmetric.cpp iff.cpp fnintern.cpp hcmplx.cpp interior.cpp express.cpp fnpovfpu.cpp image.cpp lightgrp.cpp hfield.cpp isosurf.cpp mathutil.cpp lathe.cpp matrices.cpp lbuffer.cpp normal.cpp mesh.cpp objects.cpp optout.cpp pgm.cpp parsestr.cpp octree.cpp media.cpp lighting.cpp planes.cpp point.cpp pigment.cpp parstxtr.cpp polysolv.cpp pattern.cpp povmsend.cpp polygon.cpp poly.cpp photons.cpp pov_mem.cpp povmsrec.cpp pov_util.cpp povray.cpp ppm.cpp rad_data.cpp ray.cpp quatern.cpp reswords.cpp prism.cpp quadrics.cpp radiosit.cpp parse.cpp rendctrl.cpp renderio.cpp statspov.cpp spheres.cpp splines.cpp sor.cpp super.cpp torus.cpp render.cpp targa.cpp sphsweep.cpp txttest.cpp texture.cpp userdisp.cpp triangle.cpp base/fileinputoutput.cpp userio.cpp vlbuffer.cpp warps.cpp base/stringutilities.cpp tokenize.cpp frontend/defaultplatformbase.cpp vbuffer.cpp base/povmscpp.cpp base/textstream.cpp base/processoptions.cpp base/textstreambuffer.cpp frontend/renderfrontend.cpp truetype.cpp frontend/processrenderoptions.cpp frontend/defaultrenderfrontend.cpp frontend/messageoutput.cpp base/povms.cpp \
  -o povray.wasm
#wasm-ld: error: /tmp/bsphere-6da956.o: undefined symbol: spec_qsort
#wasm-ld: error: /tmp/bsphere-6da956.o: undefined symbol: spec_qsort
#wasm-ld: error: /tmp/bsphere-6da956.o: undefined symbol: spec_qsort
#wasm-ld: error: /tmp/bsphere-6da956.o: undefined symbol: spec_qsort
#wasm-ld: error: /tmp/povray-8c2e52.o: undefined symbol: __cxa_allocate_exception
#wasm-ld: error: /tmp/povray-8c2e52.o: undefined symbol: __cxa_throw
#wasm-ld: error: /tmp/povray-8c2e52.o: undefined symbol: __cxa_allocate_exception
#wasm-ld: error: /tmp/povray-8c2e52.o: undefined symbol: __cxa_throw
#wasm-ld: error: /tmp/bsphere-6da956.o: undefined symbol: spec_qsort
#wasm-ld: error: /tmp/bsphere-6da956.o: undefined symbol: spec_qsort
#wasm-ld: error: /tmp/userio-ebc7e8.o: undefined symbol: system
#wasm-ld: error: /tmp/povray-8c2e52.o: undefined symbol: __cxa_allocate_exception
#wasm-ld: error: /tmp/povray-8c2e52.o: undefined symbol: __cxa_throw
#wasm-ld: error: /tmp/povray-8c2e52.o: undefined symbol: __cxa_allocate_exception
#wasm-ld: error: /tmp/povray-8c2e52.o: undefined symbol: __cxa_throw
#wasm-ld: error: /tmp/povray-8c2e52.o: undefined symbol: __cxa_allocate_exception
#wasm-ld: error: /tmp/povray-8c2e52.o: undefined symbol: __cxa_throw
#wasm-ld: error: /tmp/povray-8c2e52.o: undefined symbol: __cxa_allocate_exception
#wasm-ld: error: /tmp/povray-8c2e52.o: undefined symbol: __cxa_throw
#wasm-ld: error: /tmp/povray-8c2e52.o: undefined symbol: __cxa_allocate_exception
#wasm-ld: error: too many errors emitted, stopping now (use -error-limit=0 to see all errors)
#clang++: error: linker command failed with exit code 1 (use -v to see invocation)

runcpu --config=truejit --tune=base --action=build 519.lbm_r
runcpu --config=truejit --tune=base --action=build 523.xalancbmk_r
cd /tmp/spec/benchspec/CPU/523.xalancbmk_r/build/build_base_truejit-m64.0000
/opt/wasi-sdk/bin/clang++ \
  -z stack-size=$((1 << 20)) \
  -m32 \
  -D_WASI_EMULATED_SIGNAL -lwasi-emulated-signal -D_WASI_EMULATED_PROCESS_CLOCKS -lwasi-emulated-process-clocks -D_WASI_EMULATED_MMAN -lwasi-emulated-mman -D_WASI_EMULATED_GETPID -lwasi-emulated-getpid \
  -DSPEC -DNDEBUG  -DAPP_NO_THREADS -DXALAN_INMEM_MSG_LOADER -I. -Ixercesc -Ixercesc/dom -Ixercesc/dom/impl -Ixercesc/sax -Ixercesc/util/MsgLoaders/InMemory -Ixercesc/util/Transcoders/Iconv -Ixalanc/include -DPROJ_XMLPARSER -DPROJ_XMLUTIL -DPROJ_PARSERS -DPROJ_SAX4C -DPROJ_SAX2 -DPROJ_DOM -DPROJ_VALIDATORS -DXML_USE_INMEM_MESSAGELOADER -DSPEC_AUTO_SUPPRESS_OPENMP -g -O3 -DSPEC_LINUX     -DSPEC_LP64  \
  CSetDefs.cpp AIXPlatformUtils.cpp Win32PlatformUtils.cpp Win32TransService2.cpp SunCCDefs.cpp Win32MsgLoader.cpp SolarisPlatformUtils.cpp Win32TransService.cpp MIPSproDefs.cpp IRIXPlatformUtils.cpp HPCCDefs.cpp HPPlatformUtils.cpp GCCDefs.cpp AVTPart.cpp ASCIIRangeFactory.cpp AbstractNumericValidator.cpp LinuxPlatformUtils.cpp AnySimpleTypeDatatypeValidator.cpp AnyURIDatatypeValidator.cpp AVTPartSimple.cpp AVTPartXPath.cpp AttrMapImpl.cpp AttrNSImpl.cpp AllContentModel.cpp AVT.cpp AttrImpl.cpp AbstractNumericFacetValidator.cpp BMPattern.cpp BinFileInputStream.cpp BinInputStream.cpp AbstractStringValidator.cpp Base64BinaryDatatypeValidator.cpp BinFileOutputStream.cpp BinOutputStream.cpp BinMemInputStream.cpp BinMemOutputStream.cpp Base64.cpp BlockRangeFactory.cpp CDATASectionImpl.cpp BitSet.cpp CharToken.cpp BooleanDatatypeValidator.cpp ChildNode.cpp CharacterDataImpl.cpp AbstractDOMParser.cpp ClosureToken.cpp CMAny.cpp CommentImpl.cpp ConcatToken.cpp ConditionToken.cpp CMBinaryOp.cpp CMUnaryOp.cpp AttributeListImpl.cpp ContentLeafNameTypeVector.cpp ContentSpecNode.cpp DOMAttrImpl.cpp AttributesImpl.cpp Constants.cpp DOMAttrNSImpl.cpp DOMAttrMapImpl.cpp DOMChildNode.cpp DOMCDATASectionImpl.cpp DOMCharacterDataImpl.cpp DOMBuilderImpl.cpp DOMCommentImpl.cpp DOMConfigurationImpl.cpp DOMDeepNodeListImpl.cpp DOMDocumentFragmentImpl.cpp DFAContentModel.cpp CountersTable.cpp DOMEntityReferenceImpl.cpp DOMElementNSImpl.cpp ComplexTypeInfo.cpp DOMEntityImpl.cpp DOMErrorImpl.cpp DGXMLScanner.cpp DOMDocumentTypeImpl.cpp DOMException.cpp DOMLocatorImpl.cpp DOMImplementationRegistry.cpp DOMElementImpl.cpp DOMNodeListImpl.cpp DOMNodeIDMap.cpp DOMImplementationImpl.cpp DOMNodeIteratorImpl.cpp DOMNodeVector.cpp DOMNamedNodeMapImpl.cpp DOMRangeException.cpp DOMNotationImpl.cpp DOMParentNode.cpp DOMNodeImpl.cpp DOMProcessingInstructionImpl.cpp DOMStringPool.cpp DOMNormalizer.cpp DOMSupport.cpp DOMString.cpp DOMDocumentImpl.cpp DOMParser.cpp DOMServices.cpp DOMRangeImpl.cpp DOMStringPrintWriter.cpp DOMTextImpl.cpp DOMTypeInfoImpl.cpp DOMTreeWalkerImpl.cpp DOMXPathException.cpp DOM_CDATASection.cpp DOM_Attr.cpp DOM_CharacterData.cpp DOMSupportException.cpp DOM_Comment.cpp DOM_DOMException.cpp DOMSupportInit.cpp DOM_DocumentFragment.cpp DOMSupportDefault.cpp DOM_DocumentType.cpp DOM_EntityReference.cpp DOM_Element.cpp DOM_Document.cpp DOM_Entity.cpp DOM_DOMImplementation.cpp DOM_NodeFilter.cpp DOM_NamedNodeMap.cpp DOM_NodeList.cpp DOM_Notation.cpp DOM_NodeIterator.cpp DOM_ProcessingInstruction.cpp DOM_RangeException.cpp DOM_Node.cpp DOM_Text.cpp DOM_XMLDecl.cpp DOM_TreeWalker.cpp DOM_Range.cpp DStringPool.cpp DTDEntityDecl.cpp DTDAttDef.cpp DOMStringHelper.cpp DOMWriterImpl.cpp DateDatatypeValidator.cpp DTDValidator.cpp DateTimeDatatypeValidator.cpp DTDAttDefList.cpp DayDatatypeValidator.cpp DatatypeValidator.cpp DateTimeValidator.cpp DeepNodeListImpl.cpp DefaultPanicHandler.cpp DTDGrammar.cpp DocumentFragmentImpl.cpp DomMemDebug.cpp DocumentTypeImpl.cpp Dummy.cpp DecimalDatatypeValidator.cpp DurationDatatypeValidator.cpp DTDElementDecl.cpp ENTITYDatatypeValidator.cpp DoubleDatatypeValidator.cpp DocumentImpl.cpp DatatypeValidatorFactory.cpp DoubleSupport.cpp ElemApplyImport.cpp ElemApplyTemplates.cpp DTDScanner.cpp ElemAttributeSet.cpp ElemCallTemplate.cpp ElemChoose.cpp ElemComment.cpp ElemAttribute.cpp ElemCopy.cpp ElemCopyOf.cpp ElemEmpty.cpp ElemElement.cpp ElemDecimalFormat.cpp ElemExtensionCall.cpp ElemFallback.cpp ElemForwardCompatible.cpp ElemIf.cpp ElemOtherwise.cpp ElemMessage.cpp ElemLiteralResult.cpp ElemPI.cpp ElemForEach.cpp ElemParam.cpp ElemStack.cpp ElemSort.cpp ElemTemplate.cpp ElemTextLiteral.cpp ElemText.cpp ElementDefinitionImpl.cpp ElemValueOf.cpp ElemUse.cpp ElementNSImpl.cpp ElemTemplateElement.cpp ElementImpl.cpp ElemWhen.cpp ExecutionContext.cpp EncodingValidator.cpp EntityImpl.cpp EntityReferenceImpl.cpp ElemVariable.cpp ElemNumber.cpp FieldActivator.cpp FieldValueMap.cpp ElemWithParam.cpp FileHandleImpl.cpp FormatterToDeprecatedXercesDOM.cpp FloatDatatypeValidator.cpp FormatterListener.cpp ElementPrefixResolverProxy.cpp FormatterStringLengthCounter.cpp FormatterToNull.cpp FormatterToDOM.cpp FormatterToText.cpp FormatterTreeWalker.cpp FormatterToXercesDOM.cpp FormatterToHTML.cpp ExtensionNSHandler.cpp FormatterToSourceTree.cpp FunctionConcat.cpp ExtensionFunctionHandler.cpp Function.cpp FunctionContains.cpp FunctionCurrent.cpp FunctionDifference.cpp FunctionElementAvailable.cpp FunctionEvaluate.cpp FunctionFormatNumber.cpp FunctionFunctionAvailable.cpp FormatterToXML.cpp FunctionDocument.cpp FunctionGenerateID.cpp FunctionHasSameNodes.cpp FunctionIntersection.cpp FunctionLang.cpp FunctionNamespaceURI.cpp FunctionKey.cpp FunctionID.cpp FunctionNodeSet.cpp FunctionStartsWith.cpp FunctionDistinct.cpp FunctionNormalizeSpace.cpp FunctionString.cpp FunctionSubstring.cpp FunctionSubstringAfter.cpp FunctionSubstringBefore.cpp HashPtr.cpp Grammar.cpp HashXMLCh.cpp FunctionSystemProperty.cpp HeaderDummy.cpp HexBin.cpp HexBinaryDatatypeValidator.cpp FunctionUnparsedEntityURI.cpp GenerateEvent.cpp IC_Key.cpp IC_Field.cpp IC_KeyRef.cpp IC_Unique.cpp ICUResHandler.cpp FunctionTranslate.cpp IDDatatypeValidator.cpp GeneralAttributeCheck.cpp GrammarResolver.cpp IC_Selector.cpp IDREFDatatypeValidator.cpp InputSource.cpp InMemMsgLoader.cpp KVStringPair.cpp IconvTransService.cpp IdentityConstraintHandler.cpp LocalFileFormatTarget.cpp LocalFileInputSource.cpp InMemHandler.cpp MemBufFormatTarget.cpp Match.cpp MemoryManagerArrayImpl.cpp MemBufInputSource.cpp MemoryManagerImpl.cpp ListDatatypeValidator.cpp IdentityConstraint.cpp ModifierToken.cpp MonthDatatypeValidator.cpp MonthDayDatatypeValidator.cpp Mutexes.cpp MsgFileOutputStream.cpp NCNameDatatypeValidator.cpp NOTATIONDatatypeValidator.cpp NLSHandler.cpp NameDatatypeValidator.cpp MixedContentModel.cpp NamespaceScope.cpp NamedNodeMapImpl.cpp NodeIDMap.cpp NodeImpl.cpp NodeListImpl.cpp NodeIteratorImpl.cpp NamedNodeMapAttributeList.cpp NodeRefListBase.cpp IGXMLScanner.cpp MutableNodeRefList.cpp IGXMLScanner2.cpp NodeVector.cpp NodeSortKey.cpp NotationImpl.cpp NodeRefList.cpp OpFactory.cpp Op.cpp PSVIAttribute.cpp NodeNameTreeWalker.cpp NullPrintWriter.cpp PSVIElement.cpp PanicHandler.cpp PSVIItem.cpp ParenToken.cpp PSVIAttributeList.cpp ParentNode.cpp PrefixResolver.cpp PlatformUtils.cpp ParserForXMLSchema.cpp PlatformSupportInit.cpp PrintWriter.cpp ProblemListener.cpp ProcessingInstructionImpl.cpp RangeFactory.cpp KeyTable.cpp QNameDatatypeValidator.cpp ProblemListenerDefault.cpp QName.cpp RefCountedImpl.cpp RangeToken.cpp RangeTokenMap.cpp RegxUtil.cpp Resettable.cpp ReaderMgr.cpp NamespacesHandler.cpp OutputContextStack.cpp RangeImpl.cpp RegxParser.cpp SAXException.cpp SAXParseException.cpp SAX2Handler.cpp SAX2XMLFilterImpl.cpp NodeSorter.cpp RegularExpression.cpp SAXParser.cpp SchemaAttDef.cpp SchemaSymbols.cpp SchemaAttDefList.cpp SAX2XMLReaderImpl.cpp SchemaInfo.cpp SchemaGrammar.cpp StdBinInputStream.cpp SchemaElementDecl.cpp StdInInputSource.cpp StdOutFormatTarget.cpp SelectionEvent.cpp StringToken.cpp SimpleContentModel.cpp StringDatatypeValidator.cpp StringPool.cpp ResultNamespacesStack.cpp StringTokenizer.cpp StylesheetExecutionContext.cpp StylesheetConstructionContext.cpp SchemaValidator.cpp SubstitutionGroupComparator.cpp SynchronizedStringPool.cpp TextImpl.cpp TimeDatatypeValidator.cpp SGXMLScanner.cpp Token.cpp TraceListener.cpp TokenFactory.cpp TopLevelArg.cpp TracerEvent.cpp TraceListenerDefault.cpp TreeWalker.cpp TransService.cpp TreeWalkerImpl.cpp URLInputSource.cpp UnicodeRangeFactory.cpp StylesheetConstructionContextDefault.cpp URISupport.cpp UnionToken.cpp ValidationContextImpl.cpp UnionDatatypeValidator.cpp StylesheetRoot.cpp ValueStore.cpp VecAttrListImpl.cpp ValueStoreCache.cpp StylesheetHandler.cpp Wrapper4DOMInputSource.cpp Wrapper4InputSource.cpp Stylesheet.cpp XML256TableTranscoder.cpp VecAttributesImpl.cpp XML256TableTranscoder390.cpp XML88591Transcoder.cpp Writer.cpp XML88591Transcoder390.cpp XMLASCIITranscoder.cpp XMLASCIITranscoder390.cpp XMLAttDefList.cpp XMLAttDef.cpp XBoolean.cpp XMLAttr.cpp XMLBuffer.cpp XMLBigInteger.cpp XMLBigDecimal.cpp XMLBufferMgr.cpp XMLCanRepGroup.cpp XMLAbstractDoubleFloat.cpp XMLContentModel.cpp XMLChTranscoder.cpp WFXMLScanner.cpp XMLDTDDescription.cpp XMLDTDDescriptionImpl.cpp XMLDouble.cpp StylesheetExecutionContextDefault.cpp XMLDeclImpl.cpp XMLEBCDICTranscoder.cpp XMLEBCDICTranscoder390.cpp XMLChar.cpp XMLFloat.cpp XMLEntityDecl.cpp XMLElementDecl.cpp XMLException.cpp XMLGrammarDescription.cpp XMLIBM1047Transcoder.cpp XMLIBM1047Transcoder390.cpp XMLFormatter.cpp XMLInitializer.cpp XMLIBM1140Transcoder.cpp XMLIBM1140Transcoder390.cpp XMLMsgLoader.cpp XMLNumber.cpp XMLNotationDecl.cpp XMLRecognizer.cpp XMLRangeFactory.cpp XMLRegisterCleanup.cpp XMLRefInfo.cpp VariablesStack.cpp XMLParserLiaison.cpp XMLSchemaDescription.cpp XMLScannerResolver.cpp XMLGrammarPoolImpl.cpp XMLStringTokenizer.cpp XMLReader.cpp XMLDateTime.cpp XMLUCSTranscoder.cpp XMLUTF16Transcoder.cpp XMLSchemaDescriptionImpl.cpp XMLSupportException.cpp XMLUni.cpp XMLSupportInit.cpp XMLUTF8Transcoder.cpp XMLUTF8Transcoder390.cpp XMLUniCharacter.cpp XMLWin1252Transcoder.cpp XMLScanner.cpp XMemory.cpp XMLWin1252Transcoder390.cpp XMLValidator.cpp XMLURL.cpp XMLString.cpp XNodeSet.cpp XNodeSetNodeProxy.cpp XNodeSetResultTreeFragProxy.cpp XMLUri.cpp XNodeSetBase.cpp XNodeSetAllocator.cpp XNodeSetNodeProxyAllocator.cpp XNull.cpp XObjectResultTreeFragProxy.cpp XNumber.cpp XNumberBase.cpp TraverseSchema.cpp XObjectFactory.cpp XObjectTypeCallback.cpp XObjectResultTreeFragProxyBase.cpp XNumberAllocator.cpp XObjectResultTreeFragProxyText.cpp XObject.cpp XPathConstructionContext.cpp XPathConstructionContextDefault.cpp XPathEnvSupport.cpp XPathAllocator.cpp XPathCAPI.cpp XPathFactory.cpp XPathExecutionContext.cpp XObjectFactoryDefault.cpp XPathEvaluator.cpp XPathFactoryBlock.cpp XPathInit.cpp XPathFunctionTable.cpp XPathMatcher.cpp XPathMatcherStack.cpp XPathParserException.cpp XPathSymbols.cpp XPath.cpp XProtoType.cpp XPathProcessor.cpp XPathExecutionContextDefault.cpp XPathExpression.cpp XSAnnotation.cpp XResultTreeFrag.cpp XPathFactoryDefault.cpp XSAttributeUse.cpp XSAttributeDeclaration.cpp XSAttributeGroupDefinition.cpp XSDElementNSImpl.cpp XSAXMLScanner.cpp XSDLocator.cpp XSComplexTypeDefinition.cpp XSFacet.cpp XResultTreeFragAllocator.cpp XSDErrorReporter.cpp XPathEnvSupportDefault.cpp XSElementDeclaration.cpp XSLTProcessor.cpp XSIDCDefinition.cpp XSDDOMParser.cpp XSLException.cpp XSLTInputSource.cpp XSLTProcessorEnvSupport.cpp XSLTProcessorException.cpp XSLTInit.cpp XSModelGroup.cpp XSLTResultTarget.cpp XSModelGroupDefinition.cpp XSMultiValueFacet.cpp XSNotationDeclaration.cpp XSLTProcessorEnvSupportDefault.cpp XSObject.cpp XSParticle.cpp XSTypeDefinition.cpp XSNamespaceItem.cpp XPathProcessorImpl.cpp XSSimpleTypeDefinition.cpp XSModel.cpp XSWildcard.cpp XSValue.cpp XSpan.cpp XSerializeEngine.cpp XString.cpp XStringAdapter.cpp XStringBase.cpp XSObjectFactory.cpp XStringAdapterAllocator.cpp XStringAllocator.cpp XStringCached.cpp XSLTEngineImpl.cpp XStringReference.cpp XStringCachedAllocator.cpp XTokenNumberAdapter.cpp XToken.cpp XUtil.cpp XTokenStringAdapter.cpp XStringReferenceAllocator.cpp XalanAttr.cpp XUnknown.cpp XTokenNumberAdapterAllocator.cpp XalanCDataSection.cpp XalanCharacterData.cpp XTokenStringAdapterAllocator.cpp XalanComment.cpp XalanDOMException.cpp XalanAVTAllocator.cpp XalanDOMImplementation.cpp XalanDOMInit.cpp XalanAVTPartSimpleAllocator.cpp XalanAVTPartXPathAllocator.cpp XalanCAPI.cpp XalanBitmap.cpp XalanDOMStringAllocator.cpp XalanDOMStringPool.cpp XalanCompiledStylesheetDefault.cpp XalanDOMStringCache.cpp XalanDOMStringReusableAllocator.cpp XalanDocument.cpp XalanDocumentFragment.cpp XalanDocumentFragmentNodeRefListBaseProxy.cpp XalanDefaultDocumentBuilder.cpp XalanDocumentType.cpp XalanDOMStringHashTable.cpp XalanDecimalFormatSymbols.cpp XalanDefaultParsedSource.cpp XalanEXSLTDateTime.cpp XalanEXSLTDynamic.cpp XalanEXSLTCommon.cpp XalanDOMString.cpp XTemplateSerializer.cpp XalanDiagnosticMemoryManager.cpp XalanEXSLTMath.cpp XalanEXSLTSet.cpp XalanElemApplyTemplatesAllocator.cpp XalanElemAttributeAllocator.cpp XalanElemAttributeSetAllocator.cpp XalanElemCallTemplateAllocator.cpp XalanEXSLTString.cpp XalanElemElementAllocator.cpp XalanElemLiteralResultAllocator.cpp XalanElemEmptyAllocator.cpp XalanElement.cpp XalanEmptyNamedNodeMap.cpp XalanElemTemplateAllocator.cpp XalanEntity.cpp XalanDocumentPrefixResolver.cpp XalanEntityReference.cpp XalanElemValueOfAllocator.cpp XalanElemTextLiteralAllocator.cpp XalanElemTextAllocator.cpp XalanEncodingPropertyCache.cpp XalanICUMessageLoader.cpp XalanElemVariableAllocator.cpp XalanExtensions.cpp XalanFileOutputStream.cpp XalanFStreamOutputStream.cpp XalanMemoryManagement.cpp XalanMemoryManagerDefault.cpp XalanInMemoryMessageLoader.cpp XalanMsgLib.cpp XalanNLSMessageLoader.cpp XalanNamedNodeMap.cpp XalanHTMLElementsProperties.cpp XalanNode.cpp XalanNodeList.cpp XalanNodeListDummy.cpp XalanExe.cpp XalanNodeListSurrogate.cpp XalanNotation.cpp XalanMatchPatternData.cpp XalanMessageLoader.cpp XalanNullOutputStream.cpp XalanParsedSource.cpp XalanMatchPatternDataAllocator.cpp XalanProcessingInstruction.cpp XalanNumberFormat.cpp XalanQNameByReference.cpp XalanOutputStream.cpp XalanQName.cpp XalanReferenceCountedObject.cpp XalanOutputStreamPrintWriter.cpp XalanParsedURI.cpp XalanNumberingResourceBundle.cpp XalanSimplePrefixResolver.cpp XalanSourceTreeAttr.cpp XalanSourceTreeAttrNS.cpp XalanQNameByValueAllocator.cpp XalanSourceTreeAttributeAllocator.cpp XalanQNameByValue.cpp XalanSourceTreeAttributeNSAllocator.cpp XalanSourceTreeComment.cpp XalanNamespacesStack.cpp XalanSourceTreeCommentAllocator.cpp XalanSourceTreeDOMSupport.cpp XalanFileUtility.cpp XalanSourceTreeDocumentFragment.cpp XalanSourceTreeElement.cpp XalanSourceTreeElementA.cpp XalanSourceTreeDocumentAllocator.cpp XalanSourceTreeDocumentFragmentAllocator.cpp XalanSourceTreeElementANS.cpp XalanSourceTreeContentHandler.cpp XalanSourceTreeElementAAllocator.cpp XalanSourceTreeElementANSAllocator.cpp XalanSourceTreeElementNA.cpp XalanSourceTreeElementNANS.cpp XalanSourceTreeElementNAAllocator.cpp XalanSourceTreeElementNANSAllocator.cpp XalanSourceTreeInit.cpp XalanSourceTreeHelper.cpp XalanSourceTreeProcessingInstruction.cpp XalanSourceTreeProcessingInstructionAllocator.cpp XalanSourceTreeTextIWS.cpp XalanSourceTreeText.cpp XalanText.cpp XalanSourceTreeTextAllocator.cpp XalanSourceTreeTextIWSAllocator.cpp XalanSourceTreeWrapperParsedSource.cpp XalanSpaceNodeTester.cpp XalanToXercesTranscoderWrapper.cpp XalanStdOutputStream.cpp XalanTranscodingServices.cpp XalanSourceTreeDocument.cpp XalanTransformerOutputStream.cpp XalanTransformerProblemListener.cpp XalanUTF16Transcoder.cpp XalanXMLChar.cpp XalanUTF16Writer.cpp XalanUTF8Writer.cpp XercesAttrBridge.cpp XalanSourceTreeParserLiaison.cpp XalanXPathException.cpp XercesAttributeBridgeAllocator.cpp XercesBridgeHelper.cpp XercesBridgeNavigator.cpp XercesCDATASectionBridge.cpp XalanXMLSerializerBase.cpp XercesCommentBridge.cpp XercesAttrWrapper.cpp XercesDOMException.cpp XercesAttGroupInfo.cpp XercesDOMImplementationBridge.cpp XercesAttrWrapperAllocator.cpp XercesCDATASectionWrapper.cpp XercesCommentWrapper.cpp XercesDOMFormatterWalker.cpp XercesDOMImplementationWrapper.cpp XercesDOMWalker.cpp XercesDOMWrapperException.cpp XercesDOM_NodeHack.cpp XercesDocumentBridge.cpp XercesDocumentFragmentBridge.cpp XercesDocumentTypeBridge.cpp XercesDOMParser.cpp XercesDOMSupport.cpp XercesElementBridge.cpp XercesElementBridgeAllocator.cpp XercesDOMParsedSource.cpp XercesDOMWrapperParsedSource.cpp XercesDocumentTypeWrapper.cpp XercesEntityBridge.cpp XercesEntityReferenceBridge.cpp XercesElementWildcard.cpp XercesElementWrapper.cpp XercesElementWrapperAllocator.cpp XercesEntityReferenceWrapper.cpp XercesEntityWrapper.cpp XalanXMLFileReporter.cpp XercesNamedNodeMapBridge.cpp XercesNodeListBridge.cpp XalanTransformer.cpp XercesNotationBridge.cpp XercesLiaisonXalanDOMStringPool.cpp XercesNamedNodeMapAttributeList.cpp XercesProcessingInstructionBridge.cpp XercesGroupInfo.cpp XercesTextBridge.cpp XercesNodeListWrapper.cpp XercesTextBridgeAllocator.cpp XercesNamedNodeMapWrapper.cpp XercesToXalanNodeMap.cpp XercesTreeWalker.cpp XercesDocumentWrapper.cpp XercesNotationWrapper.cpp XercesProcessingInstructionWrapper.cpp XercesWrapperHelper.cpp XercesTextWrapper.cpp XercesTextWrapperAllocator.cpp YearDatatypeValidator.cpp YearMonthDatatypeValidator.cpp XercesWrapperNavigator.cpp XercesWrapperNavigatorAllocator.cpp XalanXMLSerializerFactory.cpp XercesXPath.cpp XercesWrapperToXalanNodeMap.cpp XercesParserLiaison.cpp \
  -o xalan.wasm

runcpu --config=truejit --tune=base --action=build 525.x264_r
runcpu --config=truejit --tune=base --action=build 531.deepsjeng_r
runcpu --config=truejit --tune=base --action=build 541.leela_r
runcpu --config=truejit --tune=base --action=build 544.nab_r
runcpu --config=truejit --tune=base --action=build 548.exchange2_r
runcpu --config=truejit --tune=base --action=build 557.xz_r


# 510.parest_r: it doesn't work
# 520.omnetpp_r: <!> Error during startup: Register_Function() or cMathFunction: attempt to register function "SPEC_HYPOT" with wrong number of arguments 2, should be 3.
# 526.blender_r: it doesn't compile



export SPEC_ROOT=/tmp/spec

# 511.povray_r
cd $SPEC_ROOT/benchspec/CPU/511.povray_r/data
echo "SPEC-benchmark-test.ini SPEC-benchmark-test.pov" >test/input/control
echo "SPEC-benchmark-train.ini SPEC-benchmark-train.pov" >train/input/control
echo "SPEC-benchmark-ref.ini SPEC-benchmark-ref.pov" >refrate/input/control
cp -r all/* test/
cp -r all/* train/
cp -r all/* refrate/

# 519.lbm_r
cd $SPEC_ROOT/benchspec/CPU/519.lbm_r/data/
# for each lbm.in file, copy it to control file
find . -type f -name "lbm.in" -exec bash -c 'cp "$1" "${1/lbm.in/control}"' -- {} \;

# 523.xalancbmk_r
cd $SPEC_ROOT/benchspec/CPU/523.xalancbmk_r/data/
echo "test.xml xalanc.xsl" >test/input/control
echo "allbooks.xml xalanc.xsl" >train/input/control
echo "t5.xml xalanc.xsl" >refrate/input/control

# 525.x264_r
cd $SPEC_ROOT/benchspec/CPU/525.x264_r/data/
echo "--dumpyuv 50 --frames 156 -o BuckBunny_New.264 BuckBunny.264 1280x720" >test/input/control
echo "--dumpyuv 50 --frames 156 -o BuckBunny_New.264 BuckBunny.264 1280x720" >train/input/control
echo "--pass 1 --stats x264_stats.log --bitrate 1000 --frames 1000 -o BuckBunny_New.264 BuckBunny.264  1280x720" >refrate/input/control

# 531.deepsjeng_r
cd $SPEC_ROOT/benchspec/CPU/531.deepsjeng_r/data/
echo "ref.txt" >refrate/input/control
echo "test.txt" >test/input/control
echo "train.txt" >train/input/control

# 541.leela_r
cd $SPEC_ROOT/benchspec/CPU/541.leela_r/data/
echo "ref.sgf" >refrate/input/control
echo "test.sgf" >test/input/control
echo "train.sgf" >train/input/control

# 548.exchange2_r
cd $SPEC_ROOT/benchspec/CPU/548.exchange2_r/data/
cp -r all/* test/
cp -r all/* train/
cp -r all/* refrate/

# 557.xz_r
cd $SPEC_ROOT/benchspec/CPU/557.xz_r/data/
# take a backup of the original control files; then empty them
find . -type f -name "control" -exec bash -c 'cp "$1" "${1/control/control.orig}"' -- {} \;
# in control file replace '.tar' with '.tar.xz' and '.combined' with '.combined.xz'
find . -type f -name "control" -exec sed -i 's/\.tar/\.tar\.xz/g' {} \;
find . -type f -name "control" -exec sed -i 's/\.combined/\.combined\.xz/g' {} \;
cp -r all/* test/
cp -r all/* train/
cp -r all/* refrate/
cp -r all/* refspeed/







# install uftrace
mkdir -p ~/opt
cd ~/opt
git clone https://github.com/namhyung/uftrace
cd uftrace
yes | sudo misc/install-deps.sh
./configure
make -j$(nproc --all)
sudo make install
rm -rf ~/opt/uftrace
