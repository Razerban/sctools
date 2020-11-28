// scdis.cpp - config file disassembler for Soarer's Keyboard Converter.

#include "../common/global.h"
#include "../common/hid_tokens.h"
#include "../common/macro_tokens.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <vector>
#include <utility>
#include <string>

using namespace std;

typedef unsigned char uint8_t;
typedef unsigned short uint16_t;

FILE* fout = stdout;

const char* get_force_set(uint8_t force)
{
	static const char* sets[] = { "set1", "set2", "set3", "set2ext" };
	uint8_t force_set = force & 0x0F;
	if ( 1 <= force_set && force_set <= 4 ) {
		return sets[force_set - 1];
	}
	return "ERROR";
}

const char* get_force_protocol(uint8_t force)
{
	static const char* protocols[] = { "xt", "at" };
	uint8_t force_protocol = (force & 0xF0) >> 4;
	if ( 1 <= force_protocol && force_protocol <= 2 ) {
		return protocols[force_protocol - 1];
	}
	return "ERROR";
}

const char* get_block_type(uint8_t blk_type)
{
	static const char* blk_types[] = { "layerblock", "remapblock", "macroblock" };
	if ( 0 <= blk_type && blk_type <= 2 ) {
		return blk_types[blk_type];
	}
	return "ERROR";
}

string get_ifset(uint8_t ifset)
{
	static const char* sets[8] = { "set1", "set2", "set3", "set2ext", "INVALIDSET", "INVALIDSET", "INVALIDSET", "INVALIDSET" };
	if ( !ifset ) {
		return string("any");
	} else {
		string s;
		for ( int i = 0; i < 8; ++i ) {
			if ( ifset & 1 ) {
				s += sets[i];
				s += " ";
			}
			ifset = ifset >> 1;
		}
		return s;
	}
}

string get_macro_match_metas(uint8_t desired_meta, uint8_t matched_meta)
{
	static const char* metas[4] = { "ctrl", "shift", "alt", "gui" };
	static const char* hmetas[8] = { "lctrl", "lshift", "lalt", "lgui", "rctrl", "rshift", "ralt", "rgui" };
	uint8_t unhanded_meta = (desired_meta & ~matched_meta) & 0xF0;
	string s;
	for ( int i = 0; i < 4; ++i ) {
		uint8_t mask = (uint8_t)((1 << (i + 4)) | (1 << i));
		if ( unhanded_meta & mask ) {
			s += metas[i];
			s += " ";
			desired_meta &= ~mask;
			matched_meta &= ~mask;
		}
	}
	for ( int i = 0; i < 8; ++i ) {
		uint8_t mask = (uint8_t)(1 << i);
		if ( matched_meta & mask ) {
			if ( !(desired_meta & mask) ) {
				s += "-";
			}
			s += hmetas[i];
			s += " ";
		}
	}
	return s;
}

string get_macrostep_metas(int val)
{
	static const char* metas[8] = { "lctrl", "lshift", "lalt", "lgui", "rctrl", "rshift", "ralt", "rgui" };
	string s;
	for ( int i = 0; i < 8; ++i ) {
		if ( val & 1 ) {
			s += metas[i];
			s += " ";
		}
		val = val >> 1;
	}
	return s;
}

string get_macrostep(int cmd, int val)
{
	//fprintf(fout, "\t%s %d\n", lookup_macro_token(cmd), val);
	string s;
	if ( cmd & Q_PUSH_META ) {
		s = "PUSH_META ";
	}
	s += lookup_macro_token(cmd & ~Q_PUSH_META);
	s += " ";
	int argtype = get_macro_arg_type(cmd);
	char buffer[64];
	switch ( argtype ) {
	case MACRO_ARG_NONE:
		break;
	case MACRO_ARG_HID:
		s += lookup_hid_token(val);
		break;
	case MACRO_ARG_META:
		s += get_macrostep_metas(val);
		break;
	case MACRO_ARG_DELAY:
		//s += itoa(val, buffer, 10);
		sprintf(buffer, "%d", val);
		s += string(buffer);
		break;
	default:
		s += "INVALID";
		break;
	}
	return s;
}

int process_layerblock(const uint8_t* buf, const uint8_t* bufend)
{
	const uint8_t* p = buf;
	fprintf(fout, "layerblock\n");
	if ( bufend - p < 1 ) {
		fprintf(fout, "# ERROR: block truncated\n");
		return 1;
	}
	uint8_t n = *p++;
	fprintf(fout, "# count: %d\n", n);
	if ( bufend - p != (int)n * 2 ) {
		fprintf(fout, "# ERROR: block size mismatch\n");
		return 1;
	}
	for ( int i = 0; i < n; ++i ) {
		uint8_t fn = *p++;
		uint8_t layer = *p++;
		fprintf(fout, "\t");
		int b = 1;
		while ( fn ) {
			if ( fn & 1 ) {
				fprintf(fout, "fn%d ", b);
			}
			++b;
			fn = fn >> 1;
		}
		fprintf(fout, "%d\n", layer);
	}
	return 0;
}

int process_remapblock(const uint8_t* buf, const uint8_t* bufend)
{
	const uint8_t* p = buf;
	fprintf(fout, "remapblock\n");
	if ( bufend - p < 2 ) {
		fprintf(fout, "# ERROR: block truncated\n");
		return 1;
	}
	uint8_t layer = *p++;
	fprintf(fout, "layer %d\n", layer);
	uint8_t n = *p++;
	fprintf(fout, "# count: %d\n", n);
	if ( bufend - p != (int)n * 2 ) {
		fprintf(fout, "# ERROR: block size mismatch\n");
		return 1;
	}
	for ( int i = 0; i < n; ++i ) {
		uint8_t from_hid = *p++;
		uint8_t to_hid = *p++;
		fprintf(fout, "\t%s %s\n", lookup_hid_token(from_hid), lookup_hid_token(to_hid));
	}
	return 0;
}

int process_macro(const uint8_t* buf, const uint8_t* /*bufend*/)
{
	// todo: use bufend to check length
	const uint8_t* p = buf;
	uint8_t hid_code = *p++;
	uint8_t desired_meta = *p++;
	uint8_t matched_meta = *p++;
	uint8_t press_flags = *p++;
	uint8_t release_flags = *p++;
	size_t press_length = press_flags & 0x3F;
	size_t release_length = release_flags & 0x3F;
	string s = get_macro_match_metas(desired_meta, matched_meta);
	fprintf(fout, "macro %s %s # %02X %02X\n", lookup_hid_token(hid_code), s.c_str(), desired_meta, matched_meta);
	for ( int i = 0; i < (int)press_length; ++i ) {
		uint8_t cmd = *p++;
		uint8_t val = *p++;
		fprintf(fout, "\t%s\n", get_macrostep(cmd, val).c_str());
	}
	if ( release_length ) {
		fprintf(fout, "onbreak%s\n", (release_flags & 0x40) ? "" : " norestoremeta");
		for ( int i = 0; i < (int)release_length; ++i ) {
			uint8_t cmd = *p++;
			uint8_t val = *p++;
			fprintf(fout, "\t%s\n", get_macrostep(cmd, val).c_str());
		}
	}
	fprintf(fout, "endmacro\n");
	return 0;
}

int process_macroblock(const uint8_t* buf, const uint8_t* bufend)
{
	const uint8_t* p = buf;
	fprintf(fout, "macroblock\n");
	if ( bufend - p < 1 ) {
		fprintf(fout, "# ERROR: block truncated\n");
		return 1;
	}
	uint8_t n = *p++;
	fprintf(fout, "# macro count: %d\n", n);
	for ( int i = 0; i < n; ++i ) {
		if ( bufend - p < 5 ) {
			fprintf(fout, "# ERROR: block truncated\n");
			return 1;
		}
		int press_length = p[3] & 0x3F;
		int release_length = p[4] & 0x3F;
		int macro_length = 5 + 2 * (press_length + release_length);
		if ( bufend - p < macro_length ) {
			fprintf(fout, "# ERROR: block truncated\n");
			return 1;
		}
		process_macro(p, p + macro_length);
		p += macro_length;
	}
	return 0;
}

typedef int (*config_value_print_fn)(uint8_t);

struct config_item_t {
	uint8_t					code;
	const char*				name;
	config_value_print_fn	printfn;
};

int print_portpin(uint8_t value)
{
	char pin = (char)(value & 7) + '0';
	char port = (char)((value >> 3) & 7) + 'A';
	fprintf(fout, "P%c%c", port, pin);
	return 0;
}

int print_pol_portpin(uint8_t value)
{
	char pin = (char)(value & 7) + '0';
	char port = (char)((value >> 3) & 7) + 'A';
	char polarity = (char)((value >> 6) & 1) ? '+' : '-';
	fprintf(fout, "%cP%c%c", polarity, port, pin);
	return 0;
}

int print_portpin_range(uint8_t value, uint8_t count)
{
	char pin = (char)(value & 7) + '0';
	char port = (char)((value >> 3) & 7) + 'A';
	uint8_t v = 0;
	--count;
	while ( count ) {
		count >>= 1;
		v++;
	}
	char pin2 = (char)(v - 1 + (value & 7)) + '0';
	fprintf(fout, "P%c%c:%c", port, pin2, pin);
	return 0;
}


config_item_t config_items[] = {
	{ CONFIG_LED_CAPS,		"led caps",		print_pol_portpin },
	{ CONFIG_LED_SCROLL,	"led scroll",	print_pol_portpin },
	{ CONFIG_LED_NUM,		"led num",		print_pol_portpin },
};

const config_item_t* lookup_config_item_info(uint8_t config_item)
{
	for ( int i = 0; i < sizeof(config_items) / sizeof(config_item_t); ++i ) {
		if ( config_item == config_items[i].code ) {
			return &config_items[i];
		}
	}
	return 0;
}

int process_configblock(const uint8_t* buf, const uint8_t* bufend)
{
	const uint8_t* p = buf;
	fprintf(fout, "# configblock\n");
	if ( bufend - p < 1 ) {
		fprintf(fout, "# ERROR: block truncated\n");
		return 1;
	}
	uint8_t n = *p++;
	fprintf(fout, "# config count: %d\n", n);
	if ( bufend - p != (int)n * 2 ) {
		fprintf(fout, "# ERROR: block size mismatch\n");
		return 1;
	}
	for ( int i = 0; i < n; ++i ) {
		uint8_t config_item = *p++;
		uint8_t config_value = *p++;
		const config_item_t* config_item_info = lookup_config_item_info(config_item);
		if ( config_item_info ) {
			fprintf(fout, "\t%s ", config_item_info->name);
			config_item_info->printfn(config_value);
			fprintf(fout, "\n");
		} else {
			fprintf(fout, "# WARNING: config item unrecognized: %02X\n", config_item);
		}
	}
	fprintf(fout, "# "); // comment out the 'endblock' printed after returning
	return 0;
}

int process_matrixblock(const uint8_t* buf, const uint8_t* bufend)
{
	const uint8_t* p = buf;
	fprintf(fout, "matrix\n");
	if ( bufend - p < 6 ) {
		fprintf(fout, "# ERROR: block truncated\n");
		return 1;
	}
	uint8_t scan_flags = *p++;
	uint8_t debounce_flags = *p++;
	uint8_t sense_delay = *p++;
	int num_muxstrobes = *p++;
	int num_strobes = *p++;
	int num_senses = *p++;
	int num_unstrobed = *p++;
	uint8_t muxstrobe_port = 0xFF;
	uint8_t muxstrobe_enable = 0xFF;
	uint8_t muxstrobe_gate = 0xFF;
	if ( num_muxstrobes ) {
		muxstrobe_port = *p++;
		muxstrobe_enable = *p++;
		muxstrobe_gate = *p++;
	}
	int num_keys = (num_muxstrobes + num_strobes) * num_senses + num_unstrobed;
	if ( bufend - p < num_strobes + num_senses + num_unstrobed + num_keys ) {
		fprintf(fout, "# ERROR: block truncated\n");
		return 1;
	}
	vector<uint8_t> strobes;
	for ( int i = 0; i < num_strobes; ++i ) {
		strobes.push_back(*p++);
	}
	vector<uint8_t> senses;
	for ( int i = 0; i < num_senses; ++i ) {
		senses.push_back(*p++);
	}
	vector<uint8_t> unstrobed;
	for ( int i = 0; i < num_unstrobed; ++i ) {
		unstrobed.push_back(*p++);
	}
	vector<uint8_t> keys;
	for ( int i = 0; i < num_keys; ++i ) {
		keys.push_back(*p++);
	}

	fprintf(fout, "\tscanrate %d\n", (scan_flags & 7)  + 1);
	fprintf(fout, "\tstrobe_mode %d\n", (scan_flags >> 3) & 7);
	fprintf(fout, "\tsense_polarity %d\n", (scan_flags >> 6) & 1);
	//fprintf(fout, "\tunstrobed_polarity %d\n", (scan_flags >> 7) & 7);

	fprintf(fout, "\tdebounce %d\n", debounce_flags & 7);
	fprintf(fout, "\tblocking %d\n", (debounce_flags >> 4) & 1);
	fprintf(fout, "\tdebounce_method %d\n", (debounce_flags >> 5) & 1);

	fprintf(fout, "\tsense_delay %d\n", sense_delay);

	if ( num_muxstrobes ) {
		fprintf(fout, "\tmuxstrobe_port ");
		print_portpin_range(muxstrobe_port, num_muxstrobes);
		fprintf(fout, "\n");
		if ( muxstrobe_enable != 0xFF ) {
			fprintf(fout, "\tmuxstrobe_enable ");
			print_pol_portpin(muxstrobe_enable);
			fprintf(fout, "\n");
		}
		if ( muxstrobe_gate != 0xFF ) {
			fprintf(fout, "\tmuxstrobe_gate ");
			print_pol_portpin(muxstrobe_gate);
			fprintf(fout, "\n");
		}
	}

	if ( num_senses ) {
		fprintf(fout, "\tsense ");
		for ( int i = 0; i < num_senses; ++i ) {
			fprintf(fout, " ");
			print_portpin(senses[i]);
		}
		fprintf(fout, "\n");
	}

	int ikey = 0;

	if ( num_muxstrobes ) {
		for ( int i = 0; i < num_muxstrobes; ++i ) {
			fprintf(fout, "\tmuxstrobe %d", i);
			for ( int j = 0; j < num_senses; ++j ) {
				fprintf(fout, " %s", lookup_hid_token(keys[ikey++]));
			}
			fprintf(fout, "\n");
		}
	}

	for ( int i = 0; i < num_strobes; ++i ) {
		fprintf(fout, "\tstrobe ");
		print_portpin(strobes[i]);
		for ( int j = 0; j < num_senses; ++j ) {
			fprintf(fout, " %s", lookup_hid_token(keys[ikey++]));
		}
		fprintf(fout, "\n");
	}

	for ( int i = 0; i < num_unstrobed; ++i ) {
		fprintf(fout, "\tunstrobed ");
		print_pol_portpin(unstrobed[i]);
		fprintf(fout, " %s", lookup_hid_token(keys[ikey++]));
		fprintf(fout, "\n");
	}
	return 0;
}

int process_block(const uint8_t* buf, size_t buflen)
{
	const uint8_t* p = buf;
	const uint8_t* bufend = buf + buflen;
	if ( bufend - p < 2 ) {
		fprintf(fout, "# ERROR: block truncated\n");
		return 1;
	}
	fprintf(fout, "\n# block length: %d\n", *p++);
	uint8_t blktype = *p & 0x07;
	uint8_t blksel = (*p >> 3) & 0x07;
	uint8_t has_set = (*p & 0x40) ? 1 : 0;
	uint8_t has_id = (*p & 0x80) ? 2 : 0;
	++p;
	if ( bufend - p < has_set + has_id ) {
		fprintf(fout, "# ERROR: block truncated\n");
		return 1;
	}
	uint8_t blkset = 0;
	if ( has_set ) {
		blkset = *p++;
	}
	uint16_t blkid = 0;
	if ( has_id ) {
		blkid = (uint16_t)(*p++);
		blkid |= ((uint16_t)(*p++) << 8);
	}
	fprintf(fout, "ifset %s\n", get_ifset(blkset).c_str());
	if ( has_id ) {
		fprintf(fout, "ifkeyboard %04X\n", blkid);
	} else {
		fprintf(fout, "ifkeyboard any\n");
	}
	if ( blksel ) {
		fprintf(fout, "ifselect %d\n", blksel);
	} else {
		fprintf(fout, "ifselect any\n");
	}
// 	fprintf(fout, "%s\n", get_block_type(blktype));
	int retval = 1;
	switch ( blktype ) {
	case BLOCK_LAYERDEF:	retval = process_layerblock(p, bufend); break;
	case BLOCK_REMAP:		retval = process_remapblock(p, bufend); break;
	case BLOCK_MACRO:		retval = process_macroblock(p, bufend); break;
	case BLOCK_CONFIG:		retval = process_configblock(p, bufend); break;
	case BLOCK_MATRIX:		retval = process_matrixblock(p, bufend); break;
	// todo: add default case which dumps block as hex in comment form
	}
	fprintf(fout, "end\n");
	return retval;
}

int process_file(const uint8_t* buf, size_t buflen)
{
	const uint8_t* p = buf;

	fprintf(fout, "# length: %u\n", buflen);

	// header...
	uint8_t sig1 = *p++;
	uint8_t sig2 = *p++;
	fprintf(fout, "# signature: %c %c\n", sig1, sig2);
	uint8_t ver1 = *p++;
	uint8_t ver2 = *p++;
	fprintf(fout, "# version: %d %d\n", ver1, ver2);
	if ( *p & 0x0F ) {
		fprintf(fout, "force %s\n", get_force_set(*p));
	}
	if ( *p & 0xF0 ) {
		fprintf(fout, "force %s\n", get_force_protocol(*p));
	}
	++p; // force
	++p; // reserved

	// blocks...
	int err = 0;
	const uint8_t* pend = buf + buflen;
	while ( p < pend ) {
		uint8_t blklen = *p;
		if ( !blklen ) {
			fprintf(fout, "ERROR: block length is zero!\n");
			return 1;
		}
		err |= process_block(p, blklen);
		p += blklen;
	}

	return err;
}

int main(int argc, char** argv)
{
	if ( argc != 2 ) { // don't print version if output is stdout
		printf("scdis v1.20\n");
	}

	if ( argc != 2 && argc != 3 ) {
		fprintf(stderr, "usage: scdis <binary_config> [<text_config>]\n");
		return 1;
	}
	const size_t bufsize = 16384;
	static uint8_t buf[bufsize];
	FILE* f = fopen(argv[1], "rb");
	if ( !f ) {
		fprintf(stderr, "error: could not open input file %s\n", argv[1]);
		return 1;
	}
	size_t buflen = fread(buf, 1, bufsize, f);
	fclose(f);
	if ( argc == 3 ) {
		fout = fopen(argv[2], "wt");
		if ( !fout ) {
			fprintf(stderr, "error: could not open output file %s\n", argv[2]);
			return 1;
		}
	}
	int err = process_file(buf, buflen);
	if ( fout != stdout ) {
		fclose(fout);
	}
	if ( err ) {
		fprintf(stderr, "errors encountered, see output file\n");
		return 1;
	} else if ( fout != stdout ) {
		fprintf(stderr, "No errors. Wrote: %s\n", argv[2]);
	}
	return 0;
}
