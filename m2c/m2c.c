/*
 *  Multi2Sim
 *  Copyright (C) 2012  Rafael Ubal (ubal@ece.neu.edu)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <assert.h>
#include <getopt.h>

#include <m2c/amd/amd.h>
#include <m2c/cl2llvm/cl2llvm.h>
#include <m2c/frm2bin/frm2bin.h>
#include <m2c/llvm2si/llvm2si.h>
#include <m2c/si2bin/si2bin.h>
#include <lib/mhandle/mhandle.h>
#include <lib/util/debug.h>
#include <lib/util/misc.h>
#include <lib/util/list.h>
#include <lib/util/string.h>



/*
 * Global Variables
 */

/* Output file name passed with option '-o' */
char clcc_out_file_name[MAX_STRING_SIZE];

int clcc_amd_run;  /* Run AMD native compiler */
int clcc_preprocess_run;  /* Run stand-alone preprocessor */
int clcc_cl2llvm_run;  /* Run OpenCL-to-LLVM stand-alone front-end */
int clcc_frm2bin_run;  /* Run Fermi stand-alone assembler */
int clcc_llvm2si_run;  /* Run LLVM-to-SI stand-alone back-end */
int clcc_si2bin_run;  /* Run Southern Islands stand-alone assembler */
int clcc_opt_level = 1;  /* Optimization level */


/* File names computed from source files */
struct list_t *clcc_source_file_list;  /* Source file names */
struct list_t *clcc_clp_file_list;  /* Preprocessed source list, extension '.clp' */
struct list_t *clcc_llvm_file_list;  /* LLVM files, extension, '.llvm' */
struct list_t *clcc_asm_file_list;  /* Assembly files, extension '.s' */
struct list_t *clcc_bin_file_list;  /* Binary files, extension '.bin' */

/* List of macros passed with '-D' options in the command line. */
struct list_t *clcc_define_list;  /* Elements of type 'char *' */





static char *syntax =
	"\n"
	"Syntax:\n"
	"\n"
	"\tm2s-clcc [<options>] <sources>\n"
	"\n"
	"Options:\n"
	"\n"
	"--amd\n"
	"\tUse AMD's OpenCL driver installed on the machine to compile the\n"
	"\tsources. This tool will act as a command-line wrapper of the native\n"
	"\tAMD compiler.\n"
	"\n"
	"--amd-list, -l\n"
	"\tPrint a list of available devices for the native AMD driver. This\n"
	"\toption should be used together with option '--amd'.\n"
	"\n"
	"--amd-device <device1>[,<device2>...], -d <device1>[,<device2>...]\n"
	"\tSelect a list of target devices for native AMD compilation. There\n"
	"\tshould be no spaces between device names/identifiers when separated\n"
	"\tby commas. When more than one device is selected, all binaries are\n"
	"\tpacked into one single Multi2Sim-specific ELF binary format.\n"
	"\tThis option must be combined with option '--amd-.\n"
	"\n"
	"--amd-dump-all, -a\n"
	"\tDump all intermediate file is generated during compilation. This\n"
	"\toption must be used together with '--amd'.\n"
	"\n"
	"--cl2llvm\n"
	"\tRun stand-alone OpenCL C to LLVM front-end, consuming OpenCL C\n"
	"\tsource files and generating LLVM outputs with the '.llvm'\n"
	"\tfile extension.\n"
	"\n"
	"--define <symbol>=<value>, -D <symbol>=<value>\n"
	"\tAdd a definition for additional symbols, equivalent to #define\n"
	"\tcompiler directives. This argument can be used multiple times.\n"
	"\n"
	"--frm-asm\n"
	"\tTreat the input files as source files containing Fermi assembly\n"
	"\tcode. Run the Fermi assembler and generate a CUDA kernel binary.\n"
	"\n"
	"--help, -h\n"
	"\tShow help message with command-line options.\n"
	"\n"
	"--llvm2si\n"
	"\tInterpret sources as LLVM binaries and generate Southern Islands\n"
	"\tassembly output in a '.s' file.\n"
	"\n"
	"-o <file>\n"
	"\tOutput kernel binary. If no output file is specified, each kernel\n"
	"\tsource is compiled into a kernel binary with the same name but\n"
	"\tusing the '.bin' extension.\n"
	"\n"
	"-O <level> (-O1 default)\n"
	"\tOptimization level. Supported values are:\n"
	"\t  -O0    No optimizations.\n"
	"\t  -O1    Optimizations at the LLVM level.\n"
	"\n"
	"--preprocess, -E\n"
	"\tRun the stand-alone C preprocessor. This command is equivalent to\n"
	"\tan external call to command 'cpp', replacing compiler directives\n"
	"\tand macros.\n"
	"\n"
	"--si-asm\n"
	"\tTreat the input files as source files containing Southern Islands\n"
	"\tassembly code. Run the Southern Islands assembler and generate a\n"
	"\tkernel binary.\n"
	"\n";



static void clcc_process_option(const char *option, char *optarg)
{
	/*
	 * Native AMD related options
	 */
	
	if (!strcmp(option, "amd"))
	{
		clcc_amd_run = 1;
		return;
	}

	if (!strcmp(option, "amd-dump-all") || !strcmp(option, "a"))
	{
		amd_dump_all = 1;
		return;
	}

	if (!strcmp(option, "amd-device") || !strcmp(option, "d"))
	{
		amd_device_name = optarg;
		return;
	}

	if (!strcmp(option, "define") || !strcmp(option, "D"))
	{
		list_add(clcc_define_list, xstrdup(optarg));
		return;
	}

	if (!strcmp(option, "amd-list") || !strcmp(option, "l"))
	{
		amd_list_devices = 1;
		return;
	}

	if (!strcmp(option, "cl2llvm"))
	{
		clcc_cl2llvm_run = 1;
		return;
	}

	if (!strcmp(option, "frm-asm"))
	{
		clcc_frm2bin_run = 1;
		return;
	}
	
	if (!strcmp(option, "help") || !strcmp(option, "h"))
	{
		printf("%s", syntax);
		exit(0);
	}

	if (!strcmp(option, "llvm2si"))
	{
		clcc_llvm2si_run = 1;
		return;
	}

	if (!strcmp(option, "o"))
	{
		snprintf(clcc_out_file_name, sizeof clcc_out_file_name,
				"%s", optarg);
		return;
	}

	if (!strcmp(option, "O"))
	{
		int err;
		clcc_opt_level = str_to_int(optarg, &err);
		if (err)
			fatal("%s: %s", optarg, str_error(err));
		if (!IN_RANGE(clcc_opt_level, 0, 1))
			fatal("%s: invalid value", optarg);
		return;
	}

	if (!strcmp(option, "preprocess") || !strcmp(option, "E"))
	{
		clcc_preprocess_run = 1;
		return;
	}

	if (!strcmp(option, "si-asm"))
	{
		clcc_si2bin_run = 1;
		return;
	}


	/* Option not found. Error message has been shown already by the call
	 * to 'getopts'. */
	exit(1);
}


static void clcc_read_command_line(int argc, char **argv)
{
	int option_index = 0;
	int opt;
	char option[10];

	static struct option long_options[] =
	{
		{ "amd", no_argument, 0, 0 },
		{ "amd-device", required_argument, 0, 'd' },
		{ "amd-dump-all", no_argument, 0, 'a' },
		{ "amd-list", no_argument, 0, 'l' },
		{ "cl2llvm", no_argument, 0, 0 },
		{ "frm-asm", no_argument, 0, 0},
		{ "define", required_argument, 0, 'D' },
		{ "help", no_argument, 0, 'h' },
		{ "llvm2si", no_argument, 0, 0 },
		{ "preprocess", no_argument, 0, 'E' },
		{ "si-asm", no_argument, 0, 0 },
		{ 0, 0, 0, 0 }
	};

	/* No arguments given */
	if (argc == 1)
	{
		printf("\n");
		printf("Multi2Sim " VERSION " OpenCL C Compiler\n");
		printf("Please run 'm2s-clcc --help' for a list of command-line options\n");
		printf("\n");
		exit(0);
	}
	
	/* Process options */
	while ((opt = getopt_long(argc, argv, "ad:hlo:D:EO:", long_options,
			&option_index)) != -1)
	{
		if (opt)
		{
			option[0] = opt;
			option[1] = '\0';
			clcc_process_option(option, optarg);
		}
		else
		{
			clcc_process_option(long_options[option_index].name,
				optarg);
		}
	}

	/* The rest are source files */
	while (optind < argc)
		list_add(clcc_source_file_list, xstrdup(argv[optind++]));
}


/* If a file was specified with option '-o', replace the file name in the file
 * list with the output file. The file list must contain only one element. */
static void clcc_replace_out_file_name(struct list_t *file_list)
{
	char *file_name;

	/* Nothing to do if output file name is not given */
	if (!clcc_out_file_name[0])
		return;

	/* Free old string */
	assert(list_count(file_list) == 1);
	file_name = list_get(file_list, 0);
	free(file_name);

	/* Set new name */
	file_name = xstrdup(clcc_out_file_name);
	list_set(file_list, 0, file_name);
}


static void clcc_read_source_files(void)
{
	char *file_name_ptr;
	char file_name[MAX_STRING_SIZE];
	char file_name_prefix[MAX_STRING_SIZE];
	int index;

	/* Nothing to do for no sources */
	if (!clcc_source_file_list->count)
		return;

	/* Option '-o' no allowed when multiple source files are given. */
	if (clcc_source_file_list->count > 1 && clcc_out_file_name[0])
		fatal("option '-o' not allowed when multiple sources are given");

	/* Create file names */
	LIST_FOR_EACH(clcc_source_file_list, index)
	{
		char *dot_str;
		char *slash_str;

		/* Get file name */
		file_name_ptr = list_get(clcc_source_file_list, index);

		/* Get position of last '.' after last '/' */
		dot_str = rindex(file_name_ptr, '.');
		slash_str = rindex(file_name_ptr, '/');
		if (!dot_str || slash_str > dot_str)
			dot_str = file_name_ptr + strlen(file_name_ptr);

		/* Get prefix */
		str_substr(file_name_prefix, sizeof file_name_prefix,
				file_name_ptr, 0, dot_str - file_name_ptr);

		/* Pre-processed source with '.clp' extension */
		snprintf(file_name, sizeof file_name, "%s.clp", file_name_prefix);
		list_add(clcc_clp_file_list, xstrdup(file_name));

		/* LLVM binary with '.llvm' extension */
		snprintf(file_name, sizeof file_name, "%s.llvm", file_name_prefix);
		list_add(clcc_llvm_file_list, xstrdup(file_name));

		/* Assembly code with '.s' extension */
		snprintf(file_name, sizeof file_name, "%s.s", file_name_prefix);
		list_add(clcc_asm_file_list, xstrdup(file_name));

		/* Final binary with '.bin' extension */
		snprintf(file_name, sizeof file_name, "%s.bin", file_name_prefix);
		list_add(clcc_bin_file_list, xstrdup(file_name));
	}
}


static void clcc_preprocess(struct list_t *source_file_list,
		struct list_t *clp_file_list)
{
	char cmd[MAX_LONG_STRING_SIZE];
	char *cmd_ptr;
	int cmd_size;

	char *source_file;
	char *clp_file;
	char *define;

	int index;
	int ret;
	int j;

	LIST_FOR_EACH(source_file_list, index)
	{
		/* Get files */
		source_file = list_get(source_file_list, index);
		clp_file = list_get(clp_file_list, index);
		assert(source_file);
		assert(clp_file);

		/* Initialize command */
		cmd[0] = '\0';
		cmd_ptr = cmd;
		cmd_size = sizeof cmd;
		str_printf(&cmd_ptr, &cmd_size, "cpp %s -o %s", source_file, clp_file);

		/* Add '-D' flags */
		LIST_FOR_EACH(clcc_define_list, j)
		{
			define = list_get(clcc_define_list, j);
			str_printf(&cmd_ptr, &cmd_size, " -D%s", define);
		}

		/* Check command exceeding size */
		if (!cmd_size)
			fatal("%s: 'cpp' command exceeds buffer size",
					__FUNCTION__);

		/* Run command */
		ret = system(cmd);
		if (ret == -1)
			fatal("%s: cannot run preprocessor, command 'cpp' not found",
					__FUNCTION__);

		/* Any other error by 'cpp' */
		if (ret)
			exit(ret);
	}
}


void clcc_init(void)
{
	/* List of source files */
	clcc_source_file_list = list_create();
	clcc_clp_file_list = list_create();
	clcc_llvm_file_list = list_create();
	clcc_asm_file_list = list_create();
	clcc_bin_file_list = list_create();
	clcc_define_list = list_create();

	/* Initialize compiler modules */
	cl2llvm_init();
	llvm2si_init();
	si2bin_init();
	frm2bin_init();
}


void clcc_done(void)
{
	int index;

	/* Free list of source files */
	LIST_FOR_EACH(clcc_source_file_list, index)
		free(list_get(clcc_source_file_list, index));
	list_free(clcc_source_file_list);

	/* Free list of pre-processed files */
	LIST_FOR_EACH(clcc_clp_file_list, index)
		free(list_get(clcc_clp_file_list, index));
	list_free(clcc_clp_file_list);

	/* Free list of LLVM object files */
	LIST_FOR_EACH(clcc_llvm_file_list, index)
		free(list_get(clcc_llvm_file_list, index));
	list_free(clcc_llvm_file_list);

	/* Free list of assembly files */
	LIST_FOR_EACH(clcc_asm_file_list, index)
		free(list_get(clcc_asm_file_list, index));
	list_free(clcc_asm_file_list);

	/* Free list of binary files */
	LIST_FOR_EACH(clcc_bin_file_list, index)
		free(list_get(clcc_bin_file_list, index));
	list_free(clcc_bin_file_list);

	/* Free list of '#define' directives */
	LIST_FOR_EACH(clcc_define_list, index)
		free(list_get(clcc_define_list, index));
	list_free(clcc_define_list);

	/* Finalize compiler modules */
	cl2llvm_done();
	llvm2si_done();
	si2bin_done();
	frm2bin_done();
}


int main(int argc, char **argv)
{
	/* Initialize */
	clcc_init();

	/* Read command line */
	clcc_read_command_line(argc, argv);

	/* Process list of sources in 'clcc_source_file_list' and generate the
	 * rest of the file lists. */
	clcc_read_source_files();

	/* List AMD devices */
	if (amd_list_devices)
	{
		amd_dump_device_list(stdout);
		goto out;
	}

	/* Native AMD compilation */
	if (clcc_amd_run)
	{
		clcc_replace_out_file_name(clcc_bin_file_list);
		clcc_preprocess(clcc_source_file_list, clcc_clp_file_list);
		amd_compile(clcc_clp_file_list, clcc_bin_file_list);
		goto out;
	}

	/* Stand-alone pre-processor */
	if (clcc_preprocess_run)
	{
		clcc_replace_out_file_name(clcc_clp_file_list);
		clcc_preprocess(clcc_source_file_list, clcc_clp_file_list);
		goto out;
	}

	/* OpenCL C to LLVM stand-alone front-end */
	if (clcc_cl2llvm_run)
	{
		clcc_replace_out_file_name(clcc_llvm_file_list);
		clcc_preprocess(clcc_source_file_list, clcc_clp_file_list);
		cl2llvm_compile(clcc_clp_file_list, clcc_llvm_file_list, clcc_opt_level);
		goto out;
	}

	/* LLVM to Southern Islands stand-alone back-end */
	if (clcc_llvm2si_run)
	{
		clcc_replace_out_file_name(clcc_asm_file_list);
		llvm2si_compile(clcc_source_file_list, clcc_asm_file_list);
		goto out;
	}

	/* Southern Islands assembler */
	if (clcc_si2bin_run)
	{
		clcc_replace_out_file_name(clcc_bin_file_list);
		si2bin_compile(clcc_source_file_list, clcc_bin_file_list);
		goto out;
	}

	/* Fermi assembler */
	if (clcc_frm2bin_run)
	{
		clcc_replace_out_file_name(clcc_bin_file_list);
		frm2bin_compile(clcc_source_file_list, clcc_bin_file_list);
		goto out;
	}

	/* Compilation steps */
	clcc_replace_out_file_name(clcc_bin_file_list);
	clcc_preprocess(clcc_source_file_list, clcc_clp_file_list);
	cl2llvm_compile(clcc_clp_file_list, clcc_llvm_file_list, clcc_opt_level);
	llvm2si_compile(clcc_llvm_file_list, clcc_asm_file_list);
	si2bin_compile(clcc_asm_file_list, clcc_bin_file_list);

out:
	/* Finish */
	clcc_done();
	mhandle_done();
	return 0;
}