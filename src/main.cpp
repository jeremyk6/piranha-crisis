/*
 * Copyright (c) 2020-2022 Gustavo Valiente gustavo.valiente@protonmail.com
 * zlib License, see LICENSE file.
 */

#include "bn_core.h"
#include "bn_keypad.h"
#include "bn_colors.h"
#include "bn_sprite_ptr.h"
#include "bn_sprite_actions.h"
#include "bn_sprite_animate_actions.h"
#include "bn_regular_bg_ptr.h"
#include "bn_sprite_palette_ptr.h"
#include "bn_timer.h"

#include "bn_sprite_items_pj.h"
#include "bn_affine_bg_items_bg_soft.h"
#include "bn_sprite_items_up_button.h"

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
    void update_button(bn::keypad::key_type key, bn::sprite_ptr& sprite)
    {
        bn::sprite_palette_ptr sprite_palette = sprite.palette();

        if(bn::keypad::pressed(key))
        {
            sprite_palette.set_fade(bn::colors::red, 0.5);
        }
        else if(bn::keypad::released(key))
        {
            sprite_palette.set_fade(bn::colors::orange, 0.5);
        }
        else if(bn::keypad::held(key))
        {
            sprite_palette.set_fade(bn::colors::yellow, 0.5);
        }
        else
        {
            sprite_palette.set_fade_intensity(0);
        }
    }
    
    void sprite_move(bn::sprite_ptr& sprite) {
        if(bn::keypad::left_held()) {
            sprite.set_x(sprite.x() - 1);
            sprite.set_horizontal_flip(true);
        } 
        else if(bn::keypad::right_held()) {
            sprite.set_x(sprite.x() + 1);
            sprite.set_horizontal_flip(false);
        }   
        if(bn::keypad::up_held()) {
            sprite.set_y(sprite.y() - 1);
        }
        else if(bn::keypad::down_held()) {
            sprite.set_y(sprite.y() + 1);
        }
    }

    /*void set_affine_bgs(bn::affine_bg_ptr& bg) {       
        bn::rect_window internal_window = bn::rect_window::internal();
        internal_window.set_top_left(-(bn::display::height() / 2) + 2, -1000);
        internal_window.set_bottom_right((bn::display::height() / 2) - 2, 1000);
        bn::window::outside().set_show_bg(bg, false);

        const bn::affine_bg_mat_attributes& base_attributes = bg.mat_attributes();
        bn::affine_bg_mat_attributes attributes[bn::display::height()];

        for(int index = 0, limit = bn::display::height(); index < limit; ++index) {
            attributes[index] = base_attributes;
        }

        bn::affine_bg_mat_attributes_hbe_ptr attributes_hbe =
        bn::affine_bg_mat_attributes_hbe_ptr::create(bg, attributes);

        bn::fixed base_degrees_angle;

        base_degrees_angle += 4;
        if(base_degrees_angle >= 360) {base_degrees_angle -= 360;}

        bn::fixed degrees_angle = base_degrees_angle;

        for(int index = 0, limit = bn::display::height() / 2; index < limit; ++index) {
            degrees_angle += 16;
            if(degrees_angle >= 360) {degrees_angle -= 360;}
            bn::fixed rotation_inc = bn::degrees_lut_sin(degrees_angle) * 4;
            attributes[(bn::display::height() / 2) + index].set_rotation_angle(45 + rotation_inc);
            attributes[(bn::display::height() / 2) - index - 1].set_rotation_angle(45 + rotation_inc);
        }
        attributes_hbe.reload_attributes_ref(); 
    }*/
}

bn::sprite_animate_action<4> setPJAnimation(bn::sprite_ptr pj, char animation, char speed) {
    if(animation == 0 ){
        return(bn::create_sprite_animate_action_forever(
                pj, speed, bn::sprite_items::pj.tiles_item(), 0, 3));
    }
    if(animation == 1 ){
        return(bn::create_sprite_animate_action_forever(
                pj, speed, bn::sprite_items::pj.tiles_item(), 4, 5));
    }
}

int main()
{
    bn::core::init();

    bn::affine_bg_ptr bg_soft_affine = bn::affine_bg_items::bg_soft.create_bg(0, 0);

    bn::sprite_ptr up_sprite = bn::sprite_items::up_button.create_sprite(-63, -47);

    bn::sprite_ptr pj = bn::sprite_items::pj.create_sprite(0, 0);
    bn::sprite_animate_action<4> action_move = setPJAnimation(pj, 0, 64);

    bn::timer timer = bn::timer();

    bn::sprite_text_generator text_generator(common::variable_8x16_sprite_font);
    text_generator.set_center_alignment();
    bn::vector<bn::sprite_ptr, 32> text_sprites;
    
    /* 
        State flags for the player
        0x00 = standing
        0x01 = eating
    */
    #define PJ_STATE_STANDING   0x00
    #define PJ_STATE_EATING     0x01
    #define PJ_EATING_ANIMATION_DELAY 100
    char pj_state = 0x00;
    int eating_timer = 0;

    while(true)
    {
        sprite_move(pj);
        //set_affine_bgs(bg_soft_affine);

        text_sprites.clear();
        text_generator.generate(0, 0, bn::to_string<32>(eating_timer), text_sprites);

        if(pj_state == PJ_STATE_STANDING) {
            action_move.update();
        }

        if(pj_state == PJ_STATE_EATING) {
            eating_timer++;
            if(eating_timer >= PJ_EATING_ANIMATION_DELAY) {
                action_move = setPJAnimation(pj, 0, 64);
                pj_state = PJ_STATE_STANDING;
            }
        }

        if(bn::keypad::a_pressed()) {
            action_move = setPJAnimation(pj, 1, 64);
            pj_state = PJ_STATE_EATING;
            eating_timer = 0;
        }

        action_move.update();

        bn::core::update();
    }
}
