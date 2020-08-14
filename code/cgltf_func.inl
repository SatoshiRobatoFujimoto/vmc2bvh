#include <codecvt>
#include <iomanip>
#include <sstream>
#include <fstream>

typedef struct vmc2bvh_traverse_state
{
	std::uint32_t channels_count;
	std::uint8_t  indent;
	std::wofstream* ofstream;
} vmc2bvh_traverse_state;

typedef struct vmc2bvh_options
{
	std::string rootbone;
	std::string bvhfile;
} vmc2bvh_options;



static bool char_equals_ignoreCase(char& c1, char& c2)
{
	if (c1 == c2) {
		return true;
	}
	else if (std::toupper(c1) == std::toupper(c2)) {
		return true;
	}
	return false;
}

static bool string_equals_ignoreCase(std::string& str1, std::string& str2)
{
	return ((str1.size() == str2.size()) &&
		std::equal(str1.begin(), str1.end(), str2.begin(), &char_equals_ignoreCase));
}

static bool vrm_get_root_bone(cgltf_data* data, std::string& known_name, cgltf_size* index)
{
	static std::string known_name1 = "ROOT";
	static std::string known_name2 = "ARMATURE";
	const auto node_count = data->nodes_count;
	for (cgltf_size i = 0; i < node_count; i++) {
		std::string node_name = data->nodes[i].name;
		if ((!known_name.empty() && string_equals_ignoreCase(node_name, known_name)) || 
			(string_equals_ignoreCase(node_name, known_name1) || string_equals_ignoreCase(node_name, known_name2))) 
		{
			*index = i;
			return true;
		}
	}
	return false;
}

static void bvh_indent(vmc2bvh_traverse_state* state)
{
	auto stream = state->ofstream;

	for (std::uint8_t i = 0; i < state->indent; i++) {
		*stream << "  ";
	}
}
static void bvh_traverse_bones(cgltf_node* node, vmc2bvh_traverse_state* state)
{
	auto stream = state->ofstream;

	// Treat first child of root node as a initial node because root node is not actually joining hierarchy
	// TODO: Do we need to support multiple root?
	if (node->parent == nullptr && node->children_count > 0) {
		bvh_traverse_bones(node->children[0], state);
		return;
	}
	const bool is_root = (state->channels_count == 0);
	if (is_root) {
		*stream << "HIERARCHY" << std::endl;
		*stream << "ROOT " << node->name << std::endl;
	}
	else if (node->children_count > 0) {
		bvh_indent(state);
		*stream << "JOINT " << node->name << std::endl;
	}
	else {
		bvh_indent(state);
		*stream << "End Site" << std::endl;
	}

	bvh_indent(state);
	*stream << "{" << std::endl;
	state->indent++;

	bvh_indent(state);
	*stream << "OFFSET " << std::fixed << std::setprecision(7) << node->translation[0] << " " << node->translation[1] << " " << (-node->translation[2]) << std::endl;
	if (is_root) {
		bvh_indent(state);
		*stream << "CHANNELS 6 Xposition Yposition Zposition Xrotation Yrotation Zrotation" << std::endl;
		state->channels_count += 6;
	}
	else if (node->children_count > 0) {
		bvh_indent(state);
		*stream << "CHANNELS 3 Xrotation Yrotation Zrotation" << std::endl;
		state->channels_count += 3;
	}
	for (cgltf_size i = 0; i < node->children_count; i++) {
		bvh_traverse_bones(node->children[i], state);
	}

	state->indent--;
	bvh_indent(state);
	*stream << "}" << std::endl;

	if (is_root) {
		*stream << "MOTION" << std::endl;
		*stream << "Frames: 1" << std::endl;
		*stream << "Frame Time: 0.033" << std::endl;
		*stream << std::fixed << std::setprecision(7) << node->translation[0] << " " << node->translation[1] << " " << (-node->translation[2]) << " ";
		for (std::uint32_t i = 3; i < state->channels_count; i++) {
			*stream << "0 ";
		}
		*stream << std::endl;
	}
}

static cgltf_result wstring_vrm_file_read(const struct cgltf_memory_options* memory_options, const struct cgltf_file_options* file_options, const std::wstring path, cgltf_size* size, void** data)
{
	(void)file_options;
	void* (*memory_alloc)(void*, cgltf_size) = memory_options->alloc ? memory_options->alloc : &cgltf_default_alloc;
	void (*memory_free)(void*, void*) = memory_options->free ? memory_options->free : &cgltf_default_free;

	FILE* file = _wfopen(path.c_str(), L"rb");

	if (!file)
	{
		return cgltf_result_file_not_found;
	}

	cgltf_size file_size = size ? *size : 0;

	if (file_size == 0)
	{
		fseek(file, 0, SEEK_END);

		long length = ftell(file);
		if (length < 0)
		{
			fclose(file);
			return cgltf_result_io_error;
		}

		fseek(file, 0, SEEK_SET);
		file_size = (cgltf_size)length;
	}

	char* file_data = (char*)memory_alloc(memory_options->user_data, file_size);
	if (!file_data)
	{
		fclose(file);
		return cgltf_result_out_of_memory;
	}

	cgltf_size read_size = fread(file_data, 1, file_size, file);

	fclose(file);

	if (read_size != file_size)
	{
		memory_free(memory_options->user_data, file_data);
		return cgltf_result_io_error;
	}

	if (size)
	{
		*size = file_size;
	}
	if (data)
	{
		*data = file_data;
	}

	return cgltf_result_success;
}

static cgltf_result vrm_file_read(const struct cgltf_memory_options* memory_options, const struct cgltf_file_options* file_options, const char* path, cgltf_size* size, void** data)
{
	std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
	return wstring_vrm_file_read(memory_options, file_options, converter.from_bytes(path), size, data);
}