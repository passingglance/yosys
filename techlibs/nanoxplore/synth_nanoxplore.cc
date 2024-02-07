/*
 *  yosys -- Yosys Open SYnthesis Suite
 *
 *  Copyright (C) 2012  Claire Xenia Wolf <claire@yosyshq.com>
 *  Copyright (C) 2023  Hannah Ravensloft <lofty@yosyshq.com>
 *
 *  Permission to use, copy, modify, and/or distribute this software for any
 *  purpose with or without fee is hereby granted, provided that the above
 *  copyright notice and this permission notice appear in all copies.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 *  WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 *  MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 *  ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 *  WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 *  ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 *  OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 */

#include "kernel/celltypes.h"
#include "kernel/log.h"
#include "kernel/register.h"
#include "kernel/rtlil.h"

USING_YOSYS_NAMESPACE
PRIVATE_NAMESPACE_BEGIN

struct SynthNanoXplorePass : public ScriptPass {
	SynthNanoXplorePass() : ScriptPass("synth_nanoxplore", "synthesis for NanoXplore FPGAs.") {}

	void help() override
	{
		//   |---v---|---v---|---v---|---v---|---v---|---v---|---v---|---v---|---v---|---v---|
		log("\n");
		log("    synth_nanoxplore [options]\n");
		log("\n");
		log("This command runs synthesis for NanoXplore FPGAs.\n");
		log("\n");
		log("    -top <module>\n");
		log("        use the specified module as top module\n");
		log("\n");
		log("    -json <file>\n");
		log("        write the design to the specified JSON file. writing of an output file\n");
		log("        is omitted if this parameter is not specified.\n");
		log("\n");
		log("    -noflatten\n");
		log("        do not flatten design before synthesis; useful for per-module area\n");
		log("        statistics\n");
		log("\n");
		log("    -dff\n");
		log("        pass DFFs to ABC to perform sequential logic optimisations\n");
		log("        (EXPERIMENTAL)\n");
		log("\n");
		log("    -run <from_label>:<to_label>\n");
		log("        only run the commands between the labels (see below). an empty\n");
		log("        from label is synonymous to 'begin', and empty to label is\n");
		log("        synonymous to the end of the command list.\n");
		log("\n");
		log("    -nolutram\n");
		log("        do not use LUT RAM cells in output netlist\n");
		log("\n");
		log("    -nobram\n");
		log("        do not use block RAM cells in output netlist\n");
		log("\n");
		log("    -nodsp\n");
		log("        do not map multipliers to MISTRAL_MUL cells\n");
		log("\n");
		log("    -noiopad\n");
		log("        do not instantiate IO buffers\n");
		log("\n");
		log("    -noclkbuf\n");
		log("        do not insert global clock buffers\n");
		log("\n");
		log("    -nocy\n");
		log("        do not map adders to CY cells\n");
		log("\n");
		log("The following commands are executed by this synthesis command:\n");
		help_script();
		log("\n");
	}

	string top_opt, family_opt, bram_type, vout_file, json_file;
	bool flatten, quartus, nolutram, nobram, dff, nodsp, noiopad, noclkbuf, nocy;

	void clear_flags() override
	{
		top_opt = "-auto-top";
		json_file = "";
		flatten = true;
		nolutram = false;
		nobram = false;
		dff = false;
		nodsp = false;
		noiopad = false;
		noclkbuf = false;
		nocy = false;
	}

	void execute(std::vector<std::string> args, RTLIL::Design *design) override
	{
		string run_from, run_to;
		clear_flags();

		size_t argidx;
		for (argidx = 1; argidx < args.size(); argidx++) {
			if (args[argidx] == "-top" && argidx + 1 < args.size()) {
				top_opt = "-top " + args[++argidx];
				continue;
			}
			if (args[argidx] == "-json" && argidx+1 < args.size()) {
				json_file = args[++argidx];
				continue;
			}
			if (args[argidx] == "-run" && argidx + 1 < args.size()) {
				size_t pos = args[argidx + 1].find(':');
				if (pos == std::string::npos)
					break;
				run_from = args[++argidx].substr(0, pos);
				run_to = args[argidx].substr(pos + 1);
				continue;
			}
			if (args[argidx] == "-nolutram") {
				nolutram = true;
				continue;
			}
			if (args[argidx] == "-nobram") {
				nobram = true;
				continue;
			}
			if (args[argidx] == "-nodsp") {
				nodsp = true;
				continue;
			}
			if (args[argidx] == "-noflatten") {
				flatten = false;
				continue;
			}
			if (args[argidx] == "-dff") {
				dff = true;
				continue;
			}
			if (args[argidx] == "-noiopad") {
				noiopad = true;
				continue;
			}
			if (args[argidx] == "-noclkbuf") {
				noclkbuf = true;
				continue;
			}
			if (args[argidx] == "-nocy") {
				nocy = true;
				continue;
			}
			break;
		}
		extra_args(args, argidx, design);

		if (!design->full_selection())
			log_cmd_error("This command only operates on fully selected designs!\n");

		log_header(design, "Executing SYNTH_NANOXPLORE pass.\n");
		log_push();

		run_script(design, run_from, run_to);

		log_pop();
	}

	void script() override
	{
		if (check_label("begin")) {
			run("read_verilog -specify -lib +/nanoxplore/cells_sim.v +/nanoxplore/cells_bb.v");
			// Misc and common cells
			run(stringf("hierarchy -check %s", help_mode ? "-top <top>" : top_opt.c_str()));
		}

		if (check_label("coarse")) {
			run("proc");
			if (flatten || help_mode)
				run("flatten", "(skip if -noflatten)");
			run("tribuf -logic");
			run("deminout");
			run("opt_expr");
			run("opt_clean");
			run("check");
			run("opt -nodffe -nosdff");
			run("fsm");
			run("opt");
			run("wreduce");
			run("peepopt");
			run("opt_clean");
			run("share");
			run("techmap -map +/cmp2lut.v -D LUT_WIDTH=4");
			run("opt_expr");
			run("opt_clean");
			run("alumacc");
			if (!noiopad) {
				run("iopadmap -bits -outpad $__BEYOND_OBUF I:PAD -inpad $__BEYOND_IBUF O:PAD A:top", "(only if '-iopad')");
				run("techmap -map +/nanoxplore/io_map.v");
				run("attrmvcp -attr src -attr BEL t:NX_IOB_O n:*");
				run("attrmvcp -attr src -attr BEL -driven t:NX_IOB_I n:*");
			}
			if (nocy)
				run("techmap");
			else
				run("techmap -map +/nanoxplore/arith_map.v");

			run("opt");
			run("memory -nomap");
			run("opt_clean");
		}

		/*if (!nobram && check_label("map_bram", "(skip if -nobram)")) {
			run(stringf("memory_bram -rules +/intel_alm/common/bram_%s.txt", bram_type.c_str()));
			run(stringf("techmap -map +/intel_alm/common/bram_%s_map.v", bram_type.c_str()));
		}*/

		if (!nolutram && check_label("map_lutram", "(skip if -nolutram)")) {
			run("memory_libmap -lib +/nanoxplore/drams.txt");
			run("techmap -map +/nanoxplore/cells_map.v t:$__NX_XRFB_32x36_ t:$__NX_XRFB_64x18_");
		}

		if (check_label("map_ffram")) {
			run("memory_map");
			run("opt -full");
		}

		if (check_label("map_ffs")) {
			run("techmap");
			run("dfflegalize -cell $_DFF_?P?_ 0 -cell $_ALDFF_?P_ 0 -cell $_SDFF_?P?_ 0");
			run("techmap -map +/nanoxplore/cells_map.v");
			run("opt -full -undriven -mux_undef");
			run("clean -purge");
			/*if (!noclkbuf)
				run("clkbufmap -buf MISTRAL_CLKBUF Q:A", "(unless -noclkbuf)");*/
		}

		if (check_label("map_luts")) {
			run("read_verilog -lib -icells -specify +/nanoxplore/abc9_model.v");
			run("techmap -map +/nanoxplore/abc9_map.v");
			run(stringf("abc9 %s -maxlut 4", help_mode ? "[-dff]" : dff ? "-dff" : ""));
			run("techmap -map +/nanoxplore/abc9_unmap.v");
			run("techmap -map +/nanoxplore/cells_map.v t:$lut");
			run("opt -fast");
			run("autoname");
			run("clean");
		}

		if (check_label("check")) {
			run("hierarchy -check");
			run("stat");
			run("check");
			run("blackbox =A:whitebox");
		}

		if (check_label("json"))
		{
			if (!json_file.empty() || help_mode)
				run(stringf("write_json %s", help_mode ? "<file-name>" : json_file.c_str()));
		}
	}
} SynthNanoXplorePass;

PRIVATE_NAMESPACE_END
