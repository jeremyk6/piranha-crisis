/*
 * Copyright (c) 2020-2022 Gustavo Valiente gustavo.valiente@protonmail.com
 * zlib License, see LICENSE file.
 */

#include "bn_core.h"
#include "bn_keypad.h"
#include "bn_memory.h"
#include "bn_colors.h"
#include "bn_sprite_ptr.h"
#include "bn_sprite_actions.h"
#include "bn_sprite_animate_actions.h"
#include "bn_regular_bg_ptr.h"
#include "bn_regular_bg_item.h"
#include "bn_regular_bg_map_ptr.h"
#include "bn_regular_bg_tiles_ptr.h"
#include "bn_regular_bg_map_cell_info.h"
#include "bn_sprite_palette_ptr.h"
#include "bn_timer.h"
#include "bn_camera_actions.h"

#include "bn_sprite_items_pj.h"
#include "bn_affine_bg_items_bg_soft.h"
#include "bn_regular_bg_items_lvl0.h"

#include "bn_blending.h"
#include "bn_bgs_mosaic.h"
#include "bn_rect_window.h"
#include "bn_affine_bg_ptr.h"
#include "bn_affine_bg_builder.h"
#include "bn_affine_bg_actions.h"
#include "bn_affine_bg_attributes.h"
#include "bn_affine_bg_items_bg_soft.h"
#include "bn_affine_bg_mat_attributes.h"
#include "bn_affine_bg_attributes_hbe_ptr.h"
#include "bn_affine_bg_pivot_position_hbe_ptr.h"
#include "bn_affine_bg_mat_attributes_hbe_ptr.h"

#include "common_info.h"
#include "common_variable_8x16_sprite_font.h"
#include "bn_sprite_text_generator.h"
#include "bn_string_view.h"
#include "bn_vector.h"
#include "bn_string.h"

#define GBA_SCREEN_WIDTH 240
#define GBA_SCREEN_HEIGHT 160

/* 
    State flags for the player
    0x00 = standing
    0x01 = eating
*/
#define PJ_STATE_STANDING   0x00
#define PJ_STATE_EATING     0x01
#define PJ_EATING_ANIMATION_DELAY 100
#define CAMERA_NORMAL 0
#define CAMERA_RUMBLE 1

/* 
    To reduce the display of the background if necessary (areas to be hidden)
    Good for hide collision tiles that are put in the upper left corner
*/
#define CAM_OFFSET_LEFT_CORNER 0
#define CAM_OFFSET_RIGHT_CORNER 0
#define CAM_OFFSET_UP_CORNER 0
#define CAM_OFFSET_DOWN_CORNER 0

namespace
{   
    /*
    * Sprite animations
    */

    #define PJ_ANIMATION_STAND  0
    #define PJ_ANIMATION_EAT    1
    bn::sprite_animate_action<4> pj_set_animation(bn::sprite_ptr pj, char animation, char speed) {
        if(animation == PJ_ANIMATION_EAT ){
            return(bn::create_sprite_animate_action_forever(
                    pj, speed, bn::sprite_items::pj.tiles_item(), 4, 5));
        }

        return(bn::create_sprite_animate_action_forever(
            pj, speed, bn::sprite_items::pj.tiles_item(), 0, 1, 2, 3));
    }

    void update_affine_background(bn::fixed& base_degrees_angle, bn::affine_bg_mat_attributes attributes[], bn::affine_bg_mat_attributes_hbe_ptr& attributes_hbe) {
        base_degrees_angle += 4; //"phase" (3-4)
        if(base_degrees_angle >= 360) {base_degrees_angle -= 360;}

        bn::fixed degrees_angle = base_degrees_angle;

        bn::fixed rotation_inc = 0;
        for(int index = 0, limit = bn::display::height(); index < limit; ++index) {
            degrees_angle += 16; //fréquence (16)
            if(degrees_angle >= 360) {degrees_angle -= 360;}
            rotation_inc = bn::degrees_lut_sin(degrees_angle) * 1; //amplitude (1)
            if (rotation_inc < 0) rotation_inc += 360;
            attributes[index].set_rotation_angle(rotation_inc);
        }
        attributes_hbe.reload_attributes_ref(); 
    }
    
    bn::fixed get_map_index(bn::fixed x, bn::fixed y, bn::fixed pixel_bg_wh) 
    {
        return ((x+(pixel_bg_wh/2))/8).integer() + (((y+(pixel_bg_wh/2))/8).integer()*32);
    }

    bn::fixed get_bgtile_at_pos(bn::fixed x, bn::fixed y, bn::regular_bg_ptr bg) 
    {
        bn::fixed current_cell = get_map_index(x, y, bg.dimensions().width());
        return bg.map().cells_ref().value().at(current_cell.integer());
    }

    bn::fixed bg_collision_tile_at(bn::fixed x, bn::fixed y, bn::regular_bg_ptr bg, bn::fixed query_cell)
    {
        return (get_bgtile_at_pos(x,y,bg)==query_cell);
    }

    bn::fixed lvl0_collisions(bn::fixed x, bn::fixed y, bn::regular_bg_ptr bg)
    {
        return (bg_collision_tile_at(x, y, bg, 1)==1 ||
                bg_collision_tile_at(x, y, bg, 2)==1 ||
                bg_collision_tile_at(x, y, bg, 3)==1 ||
                bg_collision_tile_at(x, y, bg, 4)==1 ||
                bg_collision_tile_at(x, y, bg, 5)==1);
    }

    void update_camera_check_edge(bn::camera_ptr camera, bn::sprite_ptr sprite, bn::regular_bg_ptr bg)
    {
        if ((sprite.x().round_integer()+bg.dimensions().width()/2) < GBA_SCREEN_WIDTH/2+CAM_OFFSET_LEFT_CORNER) //LEFT CORNER
        {
            camera.set_x(GBA_SCREEN_WIDTH/2-bg.dimensions().width()/2+CAM_OFFSET_LEFT_CORNER);
        }
        if ((sprite.x().round_integer()+bg.dimensions().width()/2) > bg.dimensions().width()-GBA_SCREEN_WIDTH/2-CAM_OFFSET_RIGHT_CORNER) //RIGHT CORNER
        {
            camera.set_x((bg.dimensions().width()-GBA_SCREEN_WIDTH/2)-bg.dimensions().width()/2-CAM_OFFSET_RIGHT_CORNER);
        }

        if ((sprite.y().round_integer()+bg.dimensions().height()/2) < GBA_SCREEN_HEIGHT/2+CAM_OFFSET_UP_CORNER) //UP CORNER
        {
            camera.set_y(GBA_SCREEN_HEIGHT/2-bg.dimensions().height()/2+CAM_OFFSET_UP_CORNER);
        }
        if ((sprite.y().round_integer()+bg.dimensions().height()/2) > bg.dimensions().height()-GBA_SCREEN_HEIGHT/2-CAM_OFFSET_DOWN_CORNER) //DOWN CORNER
        {
            camera.set_y((bg.dimensions().height()-GBA_SCREEN_HEIGHT/2)-bg.dimensions().height()/2-CAM_OFFSET_DOWN_CORNER);
        } 
    }

    void sprite_move(bn::sprite_ptr& sprite, bn::camera_ptr camera, bn::fixed& speed_x, bn::fixed& speed_y, 
                    bn::fixed& maxspeed, bn::fixed& acceleration, bn::regular_bg_ptr bg, char& pj_state, char& camera_state) {
       
       if(bn::keypad::left_held()) {
            if(speed_x >= -maxspeed) speed_x -= acceleration;
            if(speed_x <= -maxspeed) speed_x += acceleration;
             if(speed_x < 0) sprite.set_horizontal_flip(true);
        } 
        else if(bn::keypad::right_held()) {
            if(speed_x <= maxspeed) speed_x += acceleration;
            if(speed_x >= maxspeed) speed_x -= acceleration;
            if(speed_x > 0) sprite.set_horizontal_flip(false);
        }
        else {
            if(speed_x < 0)
                speed_x += acceleration;
            if(speed_x > 0)
                speed_x -= acceleration;
        }
        if(bn::keypad::up_held()) {
            if(speed_y >= -maxspeed) speed_y -= acceleration;
            if(speed_y <= -maxspeed) speed_y += acceleration;
        }
        else if(bn::keypad::down_held()) {
            if(speed_y <= maxspeed) speed_y += acceleration;
            if(speed_y >= maxspeed) speed_y -= acceleration;
        }
        else {
            if(speed_y < 0) 
                speed_y += acceleration;

            if(speed_y > 0) 
                speed_y -= acceleration;
        }
        
        bool left_collision = (lvl0_collisions(sprite.x()+speed_x*2-8, sprite.y(), bg)==1);
        bool right_collision = (lvl0_collisions(sprite.x()+speed_x*2+8, sprite.y(), bg)==1);
        bool up_collision = (lvl0_collisions(sprite.x(), sprite.y()+speed_y*2-8, bg)==1);
        bool down_collision = (lvl0_collisions(sprite.x(), sprite.y()+speed_y*2+8, bg)==1);

        if(left_collision || right_collision) {
            speed_x = -speed_x;
            if(pj_state == PJ_STATE_EATING) {
                camera_state = CAMERA_RUMBLE;
            }
        }
        if(up_collision || down_collision) {
            speed_y = -speed_y;
            if(pj_state == PJ_STATE_EATING) {
                camera_state = CAMERA_RUMBLE;
            }
        }

        sprite.set_x(sprite.x() + speed_x);
        camera.set_x(camera.x() + speed_x);
        sprite.set_y(sprite.y() + speed_y);
        camera.set_y(camera.y() + speed_y);
    }
}

int main()
{
    bn::core::init();

    /*
        Create and init regular background
    */
    bn::regular_bg_ptr lvl0 = bn::regular_bg_items::lvl0.create_bg(0, 0);
    //lvl0.put_above(); //To put above sprite !

    bn::camera_ptr camera = bn::camera_ptr::create(0, 0);

    /*
        Create and init affine background
    */
    bn::affine_bg_ptr bg_soft_affine = bn::affine_bg_items::bg_soft.create_bg(0, 0);

    bn::rect_window internal_window = bn::rect_window::internal();
    internal_window.set_top_left(-(bn::display::height() / 2), -1000);
    internal_window.set_bottom_right((bn::display::height() / 2), 1000);
    bn::window::outside().set_show_bg(bg_soft_affine, false);

    const bn::affine_bg_mat_attributes& base_attributes = bg_soft_affine.mat_attributes();
    bn::affine_bg_mat_attributes attributes[bn::display::height()];

    for(int index = 0, limit = bn::display::height(); index < limit; ++index) {
        attributes[index] = base_attributes;
    }

    bn::affine_bg_mat_attributes_hbe_ptr attributes_hbe =
    bn::affine_bg_mat_attributes_hbe_ptr::create(bg_soft_affine, attributes);

    bn::fixed base_degrees_angle;

    /*
        Create and init sprites
    */
    bn::sprite_ptr pj = bn::sprite_items::pj.create_sprite(0, 0);
    bn::sprite_animate_action<4> pj_action = pj_set_animation(pj, PJ_ANIMATION_STAND, 64);

    bn::sprite_text_generator text_generator(common::variable_8x16_sprite_font);
    text_generator.set_center_alignment();
    bn::vector<bn::sprite_ptr, 32> text_sprites;
    bn::fixed speed_x = 0;
    bn::fixed speed_y = 0;
    bn::fixed acceleration = 0.03;
    bn::fixed maxspeed = acceleration*50;

    char pj_state = PJ_STATE_STANDING;
    int eating_timer = 0;
    
    bn::fixed current_cell;

    unsigned int camera_rumble_index = 0;
    bn::fixed rumble_amplitude = 3;
    bn::fixed camera_rumble[] = {-rumble_amplitude, rumble_amplitude,-rumble_amplitude, rumble_amplitude,-rumble_amplitude, rumble_amplitude,-rumble_amplitude, rumble_amplitude,-rumble_amplitude, rumble_amplitude,-rumble_amplitude, rumble_amplitude,-rumble_amplitude, rumble_amplitude,-rumble_amplitude, rumble_amplitude,-rumble_amplitude, rumble_amplitude,-rumble_amplitude, rumble_amplitude,-rumble_amplitude, rumble_amplitude};
    char camera_state = CAMERA_NORMAL;

    pj.set_camera(camera);
    lvl0.set_camera(camera);

    while(true)
    {

        if(camera_state == CAMERA_RUMBLE){
            camera.set_position(camera.x()+camera_rumble[camera_rumble_index], camera.y()+camera_rumble[camera_rumble_index]);
            camera_rumble_index++;
            if(camera_rumble_index > (sizeof(camera_rumble) / sizeof(bn::fixed))-1) {
                camera_state = CAMERA_NORMAL;
                camera_rumble_index = 0;
            }
        }

        sprite_move(pj, camera, speed_x, speed_y, maxspeed, acceleration, lvl0, pj_state, camera_state);
        
        if(pj_state == PJ_STATE_STANDING) {
            maxspeed = acceleration*50;
            pj_action.update();
        }

        if(pj_state == PJ_STATE_EATING) {
            maxspeed = acceleration*80;
            eating_timer++;
            if(eating_timer >= PJ_EATING_ANIMATION_DELAY) {
                pj_action = pj_set_animation(pj, PJ_ANIMATION_STAND, 64);
                pj_state = PJ_STATE_STANDING;
            }
        }

        if(bn::keypad::a_pressed()) {
            pj_action = pj_set_animation(pj, PJ_ANIMATION_EAT, 64);
            pj_state = PJ_STATE_EATING;
            eating_timer = 0;
        }

        pj_action.update();

        update_affine_background(base_degrees_angle, attributes, attributes_hbe);
        
        text_sprites.clear();
        text_generator.generate(0, -40, bn::to_string<32>(get_bgtile_at_pos(pj.x(),pj.y(),lvl0)), text_sprites);
        text_generator.generate(0, 40, bn::to_string<32>(speed_x), text_sprites);
        text_generator.generate(0, -70, bn::to_string<32>(lvl0_collisions(pj.x(),pj.y(),lvl0)), text_sprites);
        
        text_generator.generate(-110, -70, bn::to_string<32>(pj.x().round_integer()), text_sprites);
        text_generator.generate(-80, -70, bn::to_string<32>(pj.y().round_integer()), text_sprites);

        update_camera_check_edge(camera, pj, lvl0); //warning put just before bn::core:update()

        bn::core::update();
    }
}
