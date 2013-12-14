#include "init.h"

#include <stdio.h>

#include "../engine/texture.h"
#include "../engine/util/strings.h"
#include "../engine/util/file.h"
#include "../engine/util/error.h"
#include "../engine/callbacks.h"
#include "../engine/util/file.h"
#include "../engine/log.h"
#include "../engine/terrain.h"

#include "callbacks.h"
#include "gamestate.h"

using namespace engine;

namespace openage {

Texture **terrain_textures, **blending_textures;

//hardcoded for now
unsigned int blend_mode_count = 9;

int *terrain_priority_list;

unsigned int terrain_data[20][20] = {
	{  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7},
	{  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7},
	{  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7},
	{  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7, 11, 11, 11,  7,  7,  7,  7,  7},
	{  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7, 11, 11, 11, 11, 11,  7,  7,  7},
	{  7,  7, 20, 20, 20,  7,  7,  7,  7,  7,  7,  7, 11, 11, 11, 11, 11, 11,  7,  7},
	{  7,  7, 20,  7,  7, 20, 20,  7,  7,  7,  7,  7, 11, 11, 11, 11, 11,  7,  7,  7},
	{  7,  7, 20,  7,  7,  7,  7,  7,  7,  7,  7,  7, 11, 11, 11,  7,  7,  7,  7,  7},
	{  7, 20, 20, 20,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7},
	{  7,  7, 20,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7},
	{  7,  7, 20,  7,  7,  7,  9,  9,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7},
	{  7,  7,  7,  7,  7,  7,  9,  9,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7},
	{  7,  7,  7,  7, 13,  7,  9,  7,  7, 12, 12,  7,  7,  7,  7,  7,  7,  7,  7,  7},
	{  7,  7,  7,  7, 13,  9,  9,  7, 12,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7},
	{  7,  7,  7,  7, 13,  7,  7,  7, 12,  7,  7,  7,  7, 17, 17, 17,  7,  7,  7,  7},
	{  7,  7,  7,  7, 13,  7,  7,  7, 12,  7,  7,  7,  7, 18, 18, 18,  7,  7,  7,  7},
	{  7,  7, 12, 12, 12, 12, 12, 12, 12,  7,  7,  7,  7, 19, 19, 19,  7,  7,  7,  7},
	{  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  3,  3,  3,  7,  7,  7,  7},
	{  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  3,  3,  3,  3, 14, 14,  7},
	{  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  3,  3,  7,  7,  7}
};

void init() {
	//load textures and stuff
	gaben = new Texture("gaben.png");

	//TODO: dynamic generation of the file path
	//sync this with convert .py script !

	university = new Texture("age/raw/Data/graphics.drs/3836.slp.png", true, PLAYERCOLORED);

	engine::TerrainType *terrain_types;
	size_t ttype_count = engine::util::read_csv_file<engine::TerrainType>(&terrain_types, "age/processed/terrain_meta.docx");

	engine::BlendingMode *blending_modes;
	size_t bmode_count = engine::util::read_csv_file<engine::BlendingMode>(&blending_modes, "age/processed/blending_meta.docx");

	terrain = new Terrain(20, ttype_count, terrain_types, bmode_count, blending_modes);



	//set the terrain types according to the data array.
	coord::tile pos = {0, 0};
	for (; pos.ne < (int) terrain->get_size(); pos.ne++) {
		for (pos.se = 0; pos.se < (int) terrain->get_size(); pos.se++) {
			int texid = terrain_data[pos.ne][pos.se];
			terrain->set_tile(pos, texid);
		}
	}


	struct player_color_line : engine::util::csv_line_data {
		unsigned int r, g, b, a;

		int fill(const char *by_line) {
			if (5 == sscanf(by_line, "%u=%u,%u,%u,%u",
			           &this->idx,
			           &this->r,
			           &this->g,
			           &this->b,
			           &this->a
			           )) {
				return 0;
			}
			else {
				return -1;
			}
		}
		void dump() {
			engine::log::msg("color %u: (%u,%u,%u,%u)", this->idx, this->r, this->g, this->b, this->a);
		}
	};


	//get the player colors from the sub-palette exported by script
	player_color_line *player_color_file;
	size_t pcolor_count = engine::util::read_csv_file<player_color_line>(&player_color_file, "age/processed/player_color_palette.pal");

	GLfloat *playercolors = new GLfloat[pcolor_count*4];
	for (size_t i = 0; i < pcolor_count; i++) {
		auto line = &player_color_file[i];
		playercolors[i*4]     = line->r / 255.0;
		playercolors[i*4 + 1] = line->g / 255.0;
		playercolors[i*4 + 2] = line->b / 255.0;
		playercolors[i*4 + 3] = line->a / 255.0;
	}


	//shader initialisation
	//read shader source codes and create shader objects for wrapping them.

	char *texture_vert_code = util::read_whole_file("shaders/maptexture.vert.glsl");
	auto plaintexture_vert = new shader::Shader(GL_VERTEX_SHADER, texture_vert_code);
	delete[] texture_vert_code;

	char *texture_frag_code = util::read_whole_file("shaders/maptexture.frag.glsl");
	auto plaintexture_frag = new shader::Shader(GL_FRAGMENT_SHADER, texture_frag_code);
	delete[] texture_frag_code;

	char *teamcolor_frag_code = util::read_whole_file("shaders/teamcolors.frag.glsl");
	auto teamcolor_frag = new shader::Shader(GL_FRAGMENT_SHADER, teamcolor_frag_code);
	delete[] teamcolor_frag_code;

	char *alphamask_vert_code = util::read_whole_file("shaders/alphamask.vert.glsl");
	auto alphamask_vert = new shader::Shader(GL_VERTEX_SHADER, alphamask_vert_code);
	delete[] alphamask_vert_code;

	char *alphamask_frag_code = util::read_whole_file("shaders/alphamask.frag.glsl");
	auto alphamask_frag = new shader::Shader(GL_FRAGMENT_SHADER, alphamask_frag_code);
	delete[] alphamask_frag_code;



	//create program for rendering simple textures
	texture_shader::program = new shader::Program(plaintexture_vert, plaintexture_frag);
	texture_shader::program->link();
	texture_shader::texture = texture_shader::program->get_uniform_id("texture");
	texture_shader::tex_coord = texture_shader::program->get_attribute_id("tex_coordinates");
	texture_shader::program->use();
	glUniform1i(texture_shader::texture, 0);
	texture_shader::program->stopusing();


	//create program for tinting textures at alpha-marked pixels
	//with team colors
	teamcolor_shader::program = new shader::Program(plaintexture_vert, teamcolor_frag);
	teamcolor_shader::program->link();
	teamcolor_shader::texture = teamcolor_shader::program->get_uniform_id("texture");
	teamcolor_shader::tex_coord = teamcolor_shader::program->get_attribute_id("tex_coordinates");
	teamcolor_shader::player_id_var = teamcolor_shader::program->get_uniform_id("player_number");
	teamcolor_shader::alpha_marker_var = teamcolor_shader::program->get_uniform_id("alpha_marker");
	teamcolor_shader::player_color_var = teamcolor_shader::program->get_uniform_id("player_color");
	teamcolor_shader::program->use();
	glUniform1i(teamcolor_shader::texture, 0);
	glUniform1f(teamcolor_shader::alpha_marker_var, 254.0/255.0);
	//fill the teamcolor shader's player color table:
	glUniform4fv(teamcolor_shader::player_color_var, 64, playercolors);
	teamcolor_shader::program->stopusing();
	delete[] playercolors;


	//create program for drawing textures that are alpha-masked before
	alphamask_shader::program = new shader::Program(alphamask_vert, alphamask_frag);
	alphamask_shader::program->link();
	alphamask_shader::base_coord = alphamask_shader::program->get_attribute_id("base_tex_coordinates");
	alphamask_shader::mask_coord = alphamask_shader::program->get_attribute_id("mask_tex_coordinates");
	alphamask_shader::show_mask = alphamask_shader::program->get_uniform_id("show_mask");
	alphamask_shader::base_texture = alphamask_shader::program->get_uniform_id("base_texture");
	alphamask_shader::mask_texture = alphamask_shader::program->get_uniform_id("mask_texture");
	alphamask_shader::program->use();
	glUniform1i(alphamask_shader::base_texture, 0);
	glUniform1i(alphamask_shader::mask_texture, 1);
	alphamask_shader::program->stopusing();

	//after linking, the shaders are no longer necessary
	delete plaintexture_vert;
	delete plaintexture_frag;
	delete teamcolor_frag;
	delete alphamask_vert;
	delete alphamask_frag;

	//setup engine callback methods
	engine::callbacks::on_input.push_back(input_handler);
	engine::callbacks::on_engine_tick.push_back(on_engine_tick);
	engine::callbacks::on_drawgame.push_back(draw_method);
	engine::callbacks::on_drawhud.push_back(hud_draw_method);
}

void destroy() {
	//oh noes, release hl3 before that!
	delete gaben;

	delete university;
	delete texture_shader::program;
	delete teamcolor_shader::program;
	delete alphamask_shader::program;

	for (unsigned int i = 0; i < terrain_texture_count; i++) {
		delete terrain_textures[i];
	}
	for (unsigned int i = 0; i < blend_mode_count; i++) {
		delete blending_textures[i];
	}
	delete[] terrain_priority_list;
}

} //namespace openage
