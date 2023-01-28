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

namespace
{   
    void sprite_move(bn::sprite_ptr& sprite, bn::camera_ptr camera, float& accel_x, float& accel_y) {
        /*
        if(bn::keypad::left_held()) {
            sprite.set_x(sprite.x() - 1);
            sprite.set_horizontal_flip(true);
            camera.set_x(camera.x() - 1);
        } 
        else if(bn::keypad::right_held()) {
            sprite.set_x(sprite.x() + 1);
            sprite.set_horizontal_flip(false);
            camera.set_x(camera.x() + 1);
        }   
        if(bn::keypad::up_held()) {
            sprite.set_y(sprite.y() - 1);
            camera.set_y(camera.y() - 1);
        }
        else if(bn::keypad::down_held()) {
            sprite.set_y(sprite.y() + 1);
            camera.set_y(camera.y() + 1);
        }
        */
       if(bn::keypad::left_held()) {
            if(accel_x >= -1) accel_x -= 0.1;
        } 
        else if(bn::keypad::right_held()) {
            if(accel_x <= 1) accel_x += 0.1;
        }
        else {
            if(accel_x < 0) accel_x += 0.1;
            if(accel_x > 0) accel_x -= 0.1;
        }
        if(bn::keypad::up_held()) {
            if(accel_y >= -1) accel_y -= 0.05;
        }
        else if(bn::keypad::down_held()) {
            if(accel_y <= 1) accel_y += 0.1;
        }
        else {
            if(accel_y < 0) accel_y += 0.05;
            if(accel_y > 0) accel_y -= 0.1;
        }

        sprite.set_x(sprite.x() + 1 * accel_x);
        sprite.set_horizontal_flip(false);
        camera.set_x(camera.x() + 1 * accel_x);
        sprite.set_y(sprite.y() + 1 * accel_y);
        camera.set_y(camera.y() + 1 * accel_y);
    }

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
            pj, speed, bn::sprite_items::pj.tiles_item(), 0, 3));
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

    bn::fixed modulo(bn::fixed a, bn::fixed m)
    {
        return a - m * ((a/m).right_shift_integer());
    }
    
    bn::fixed get_map_index(bn::fixed x, bn::fixed y, bn::fixed pixel_bg_wh)
    {
        // N_tile = (((x+map/2)/8 (int) + ((y+map/2)/8 (int) * 32))
        //return modulo((y.safe_division(8).right_shift_integer() * map_size/8 + x.safe_division(8).integer()), map_size*8);
        return ((x+(pixel_bg_wh/2))/8).integer() + (((y+(pixel_bg_wh/2))/8).integer()*32);
    }
}

int main()
{
    bn::core::init();

    /*
        Create and init regular??? background
    */
    bn::regular_bg_ptr lvl0 = bn::regular_bg_items::lvl0.create_bg(0, 0);

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
    float accel_x = 0;
    float accel_y = 0;
    
    /* 
        State flags for the player
        0x00 = standing
        0x01 = eating
    */
    #define PJ_STATE_STANDING   0x00
    #define PJ_STATE_EATING     0x01
    #define PJ_EATING_ANIMATION_DELAY 100

    char pj_state = PJ_STATE_STANDING;
    int eating_timer = 0;
    
    bn::fixed current_cell;

    while(true)
    {
        sprite_move(pj, camera, accel_x, accel_y);
        pj.set_camera(camera);
        lvl0.set_camera(camera);

        if(pj_state == PJ_STATE_STANDING) {
            pj_action.update();
        }

        if(pj_state == PJ_STATE_EATING) {
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
        
        
        /*
        Get cells
        */
        current_cell = get_map_index(pj.x(), pj.y(), 256); //256 = mapsize
        text_sprites.clear();
        text_generator.generate(0, -40, bn::to_string<32>(lvl0.map().cells_ref().value().at(current_cell.integer())), text_sprites);
        //text_generator.generate(0, -40, bn::to_string<32>(get_map_index(pj.x(), pj.y(), 256)), text_sprites);

        bn::core::update();
    }
}
