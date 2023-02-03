/*
 * Copyright (c) 2020-2022 Gustavo Valiente gustavo.valiente@protonmail.com
 * zlib License, see LICENSE file.
 */

#include "bn_core.h"
#include "bn_keypad.h"
#include "bn_sprite_ptr.h"
#include "bn_sprite_actions.h"
#include "bn_sprite_animate_actions.h"
#include "bn_sprite_tiles_item.h"
#include "bn_sprite_first_attributes.h"
#include "bn_regular_bg_ptr.h"
#include "bn_regular_bg_item.h"
#include "bn_regular_bg_map_ptr.h"
#include "bn_regular_bg_tiles_ptr.h"
#include "bn_regular_bg_map_cell_info.h"
#include "bn_sprite_palette_ptr.h"
#include "bn_camera_actions.h"

// Ressources
#include "bn_sprite_items_pj.h"
#include "bn_sprite_items_spr_lifebar.h"
#include "bn_sprite_items_spr_counter.h"
#include "bn_sprite_items_fish_normal.h"
#include "bn_sprite_items_fish_speed.h"
#include "bn_sprite_items_fish_deformation.h"
#include "bn_sprite_items_fish_occoured.h"
#include "bn_sprite_items_fish_confusion.h"
#include "bn_affine_bg_items_bg_soft.h"
#include "bn_regular_bg_items_lvl0.h"
#include "bn_music_items.h"
#include "bn_sound_items.h"

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
#include "bn_vector.h"
#include "bn_string.h"
#include "bn_random.h"
#include "bn_vector.h"

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
#define CAM_OFFSET_LEFT_LIMIT 16
#define CAM_OFFSET_RIGHT_LIMIT 16
#define CAM_OFFSET_UP_LIMIT 16
#define CAM_OFFSET_DOWN_LIMIT 16

/* 
    FISHES CLASSES
*/

#define FISH_TYPE_NORMAL        0
#define FISH_TYPE_SPEED         1
#define FISH_TYPE_CONFUSION     2
#define FISH_TYPE_DEFORMATION   3
#define FISH_TYPE_SUPER         4
#define FISH_BOXSIZE 16

#define DIRECTION_UP            0
#define DIRECTION_UP_RIGHT      1
#define DIRECTION_RIGHT         2
#define DIRECTION_DOWN_RIGHT    3
#define DIRECTION_DOWN          4
#define DIRECTION_DOWN_LEFT     5
#define DIRECTION_LEFT          6
#define DIRECTION_UP_LEFT       7
#define DIRECTION_NONE          8

class Fish {
    protected:
        bn::optional<bn::sprite_ptr> sprite;
        bn::optional<bn::sprite_animate_action<3>> animation;
        bn::camera_ptr* camera;
        bn::fixed speed;
        bn::fixed x;
        bn::fixed y;
        bn::random* random;
        //unsigned char type;
        unsigned short timer_init;
        unsigned short timer_wait;
        unsigned short timer;
        unsigned char direction;
    public:
        bn::fixed getX() {
            return(this->sprite->x());
        }
        bn::fixed getY() {
            return(this->sprite->y());
        }
        /*bn::fixed getType() {
            return(this->type);
        }*/
        bool collision(bn::fixed pj_x, bn::fixed pj_y) {
            return ((pj_x > this->sprite->x()+this->camera->x() - FISH_BOXSIZE) &&
                    (pj_x < this->sprite->x()+this->camera->x() + FISH_BOXSIZE) &&
                    (pj_y > this->sprite->y()+this->camera->y() - FISH_BOXSIZE) &&
                    (pj_y < this->sprite->y()+this->camera->y() + FISH_BOXSIZE));
        }
        void update() {
            if(direction == DIRECTION_UP) {
                this->y -= speed;
            }
            if(direction == DIRECTION_UP_RIGHT) {
                this->x += speed;
                this->y -= speed;
            }
            if(direction == DIRECTION_RIGHT) {
                this->x += speed;
            }
            if(direction == DIRECTION_DOWN_RIGHT) {
                this->x += speed;
                this->y -= speed;
            }
            if(direction == DIRECTION_DOWN) {
                this->y += speed;
            }
            if(direction == DIRECTION_DOWN_LEFT) {
                this->x -= speed;
                this->y += speed;
            }
            if(direction == DIRECTION_LEFT) {
                this->x -= speed;
            }
            if(direction == DIRECTION_UP_LEFT) {
                this->x -= speed;
                this->y -= speed;
            }

            // Déplacement du sprite
            this->sprite->set_x(this->x - this->camera->x());
            this->sprite->set_y(this->y - this->camera->y());
            
            // Gestion du timing
            this->timer -= 1;
            if(this->timer == 0) {
                if(direction == DIRECTION_NONE) {
                    direction = random->get_int(8);
                    timer = this->timer_init;
                } else {
                    direction = DIRECTION_NONE;
                    timer = this->timer_wait;
                }
            }
        }
        Fish(bn::fixed init_x, bn::fixed init_y, bn::camera_ptr& cam, bn::random& rand, bn::fixed speed_value, unsigned short timer_value, unsigned short timer_wait_value) {
            this->x = init_x;
            this->y = init_y;
            this->camera = &cam;
            this->random = &rand;
            this->speed = speed_value;
            this->timer_init = timer_value;
            this->timer_wait = timer_wait_value;
            this->timer = timer_value;
            this->direction = random->get_int(8);
        }
};

class NormalFish : public Fish {
    public : 
        NormalFish(bn::fixed init_x, bn::fixed init_y, bn::camera_ptr& cam, bn::random& rand) : Fish(init_x, init_y, cam, rand, 0.5, 100, 50) {
            this->sprite = bn::sprite_items::fish_normal.create_sprite(init_x, init_y);
        }
        unsigned char getType() {
            return(FISH_TYPE_NORMAL);
        }
};


class SpeedFish : public Fish {
    public : 
        SpeedFish(bn::fixed init_x, bn::fixed init_y, bn::camera_ptr& cam, bn::random& rand) : Fish(init_x, init_y, cam, rand, 1, 100, 20) {
            this->sprite = bn::sprite_items::fish_speed.create_sprite(init_x, init_y);
        }
        unsigned char getType() {
            return(FISH_TYPE_SPEED);
        }
};


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

    bn::fixed get_bgtile_at_pos(bn::fixed x, bn::fixed y, bn::regular_bg_map_item map_item) 
    {
        bn::fixed x_cell = x.integer() + 8*map_item.dimensions().width()/2;
        bn::fixed y_cell = y.integer() + 8*map_item.dimensions().width()/2;
        return map_item.cell((x_cell/8).integer(), (y_cell/8).integer());
    }

    bn::fixed bg_collision_tile_at(bn::fixed x, bn::fixed y, bn::regular_bg_map_item map_item, bn::fixed query_cell)
    {
        return (get_bgtile_at_pos(x,y,map_item)==query_cell);
    }

    bn::fixed lvl0_collisions(bn::fixed x, bn::fixed y, bn::regular_bg_map_item map_item)
    {
        return (bg_collision_tile_at(x, y, map_item, 1)==1 ||
                bg_collision_tile_at(x, y, map_item, 2)==1 ||
                bg_collision_tile_at(x, y, map_item, 3)==1 ||
                bg_collision_tile_at(x, y, map_item, 4)==1);
    }

    void update_camera_check_edge(bn::camera_ptr camera, bn::sprite_ptr sprite, bn::regular_bg_ptr bg)
    {
        if ((sprite.x().round_integer()+bg.dimensions().width()/2) < GBA_SCREEN_WIDTH/2+CAM_OFFSET_LEFT_LIMIT) //LEFT CORNER
        {
            camera.set_x(GBA_SCREEN_WIDTH/2-bg.dimensions().width()/2+CAM_OFFSET_LEFT_LIMIT);
        }
        if ((sprite.x().round_integer()+bg.dimensions().width()/2) > bg.dimensions().width()-GBA_SCREEN_WIDTH/2-CAM_OFFSET_RIGHT_LIMIT) //RIGHT CORNER
        {
            camera.set_x((bg.dimensions().width()-GBA_SCREEN_WIDTH/2)-bg.dimensions().width()/2-CAM_OFFSET_RIGHT_LIMIT);
        }

        if ((sprite.y().round_integer()+bg.dimensions().height()/2) < GBA_SCREEN_HEIGHT/2+CAM_OFFSET_UP_LIMIT) //UP CORNER
        {
            camera.set_y(GBA_SCREEN_HEIGHT/2-bg.dimensions().height()/2+CAM_OFFSET_UP_LIMIT);
        }
        if ((sprite.y().round_integer()+bg.dimensions().height()/2) > bg.dimensions().height()-GBA_SCREEN_HEIGHT/2-CAM_OFFSET_DOWN_LIMIT) //DOWN CORNER
        {
            camera.set_y((bg.dimensions().height()-GBA_SCREEN_HEIGHT/2)-bg.dimensions().height()/2-CAM_OFFSET_DOWN_LIMIT);
        } 
    }

    void sprite_move(bn::sprite_ptr& sprite, bn::camera_ptr camera, bn::fixed& speed_x, bn::fixed& speed_y, 
                    bn::fixed& maxspeed, bn::fixed& acceleration, bn::regular_bg_map_item map_item, char& pj_state, char& camera_state) {
       
        bool left_collision = (lvl0_collisions(sprite.x()+speed_x, sprite.y(), map_item)==1);
        bool right_collision = (lvl0_collisions(sprite.x()+speed_x, sprite.y(), map_item)==1);
        bool up_collision = (lvl0_collisions(sprite.x(), sprite.y()+speed_y, map_item)==1);
        bool down_collision = (lvl0_collisions(sprite.x(), sprite.y()+speed_y, map_item)==1);

        if(bn::keypad::left_held() && !left_collision) {
            if(speed_x >= -maxspeed) speed_x -= acceleration;
            if(speed_x <= -maxspeed) speed_x += acceleration;
             if(speed_x < 0) sprite.set_horizontal_flip(true);
        } 
        else if(bn::keypad::right_held() && !right_collision) {
            if(speed_x <= maxspeed) speed_x += acceleration;
            if(speed_x >= maxspeed) speed_x -= acceleration;
            if(speed_x > 0) sprite.set_horizontal_flip(false);
        }
        else {
            if(speed_x < 0 && !left_collision)
                speed_x += acceleration*1.5;
            if(speed_x > 0 && !right_collision)
                speed_x -= acceleration*1.5;
        }
        if(bn::keypad::up_held() && !up_collision) {
            if(speed_y >= -maxspeed) speed_y -= acceleration;
            if(speed_y <= -maxspeed) speed_y += acceleration;
        }
        else if(bn::keypad::down_held() && !down_collision) {
            if(speed_y <= maxspeed) speed_y += acceleration;
            if(speed_y >= maxspeed) speed_y -= acceleration;
        }
        else {
            if(speed_y < 0 && !down_collision) 
                speed_y += acceleration*1.5;

            if(speed_y > 0 && !up_collision) 
                speed_y -= acceleration*1.5;
        }

        if(left_collision || right_collision) {
            speed_x = -speed_x;
            if(pj_state == PJ_STATE_EATING) {
                camera_state = CAMERA_RUMBLE;
                bn::sound_items::choc.play(abs(speed_x/maxspeed));
            }
            else{
                bn::sound_items::boing.play(1);
            }
        }
        if(up_collision || down_collision) {
            speed_y = -speed_y;
            if(pj_state == PJ_STATE_EATING) {
                camera_state = CAMERA_RUMBLE;
                bn::sound_items::choc.play(abs(speed_y/maxspeed));
            }
            else{
                bn::sound_items::boing.play(1);
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
    
    bn::regular_bg_map_ptr lvl0_map = bn::regular_bg_items::lvl0.create_map();
    bn::regular_bg_map_item lvl0_map_item = bn::regular_bg_items::lvl0.map_item();
    //lvl0.put_above(); //To put above other bg !

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

    for(short index = 0, limit = bn::display::height(); index < limit; ++index) {
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

    bn::sprite_ptr lifebar = bn::sprite_items::spr_lifebar.create_sprite(16-GBA_SCREEN_WIDTH/2, 16-GBA_SCREEN_HEIGHT/2);
    bn::sprite_ptr counter = bn::sprite_items::spr_counter.create_sprite(GBA_SCREEN_WIDTH/2-16, GBA_SCREEN_HEIGHT/2-16);

    bn::sprite_text_generator text_generator(common::variable_8x16_sprite_font);
    text_generator.set_center_alignment();
    bn::vector<bn::sprite_ptr, 32> text_sprites;
    bn::fixed speed_x = 0;
    bn::fixed speed_y = 0;
    bn::fixed acceleration = 0.03;
    bn::fixed maxspeed = acceleration*50;

    char pj_state = PJ_STATE_STANDING;
    short eating_timer = 0;

    /* Random generator */
    bn::random random = bn::random();

    /*
        Life bar and scoring
    */
    bn::fixed current_life = 8; //0(min) to 8(max)
    bn::fixed decreaze_value = 0.25/60;
    int fish_points = 0;
    
    /*
        Camera
    */
    unsigned char camera_rumble_index = 0;
    short rumble_amplitude = 3;
    bn::fixed camera_rumble[] = {-rumble_amplitude, rumble_amplitude,-rumble_amplitude, rumble_amplitude,-rumble_amplitude, rumble_amplitude,-rumble_amplitude, rumble_amplitude,-rumble_amplitude, rumble_amplitude,-rumble_amplitude, rumble_amplitude,-rumble_amplitude, rumble_amplitude,-rumble_amplitude, rumble_amplitude,-rumble_amplitude, rumble_amplitude,-rumble_amplitude, rumble_amplitude,-rumble_amplitude, rumble_amplitude};
    char camera_state = CAMERA_NORMAL;

    pj.set_camera(camera);
    lvl0.set_camera(camera);

    /*
        Musique BG
    */
    bn::music_items::music.play(1);

    #define FISH_NUMBER 25
    bn::vector<Fish, FISH_NUMBER> fish_list;
    for(char i = 0; i < FISH_NUMBER; i++) {
        if(i % 2 == 0)
            fish_list.push_back(NormalFish(-64, -64, camera, random));
        else
            fish_list.push_back(SpeedFish(-64, -64, camera, random));
    }

    while(true)
    {
        
        for(char fish_index = 0; fish_index < fish_list.size(); fish_index++) {
            fish_list.at(fish_index).update();
            if(fish_list.at(fish_index).collision(pj.x(), pj.y()) && pj_state == PJ_STATE_EATING) {
                fish_list.erase(&fish_list.at(fish_index));
                current_life+=1;
                if(current_life > 8) current_life = 8;
                fish_points+=1;
            }
        }

        if(camera_state == CAMERA_RUMBLE){
            camera.set_position(camera.x()+camera_rumble[camera_rumble_index], camera.y()+camera_rumble[camera_rumble_index]);
            camera_rumble_index++;
            if(camera_rumble_index > (sizeof(camera_rumble) / sizeof(bn::fixed))-1) {
                camera_state = CAMERA_NORMAL;
                camera_rumble_index = 0;
            }
        }

        sprite_move(pj, camera, speed_x, speed_y, maxspeed, acceleration, lvl0_map_item, pj_state, camera_state);
        
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

            bn::sound_items::grunting.play(1);
        }

        pj_action.update();

        update_affine_background(base_degrees_angle, attributes, attributes_hbe);
        
        text_sprites.clear();
        //text_generator.generate(0, -40, bn::to_string<32>(sizeof(bn::fixed)), text_sprites);
        //text_generator.generate(0, 40, bn::to_string<32>(fish_list[0]->getX()), text_sprites);
        //text_generator.generate(0, -70, bn::to_string<32>(collision), text_sprites);
        //text_generator.generate(-110, -70, bn::to_string<32>(pj.x().integer() + 8*lvl0_map_item.dimensions().width()/2), text_sprites);
        

        text_generator.generate(GBA_SCREEN_WIDTH/2-8, GBA_SCREEN_HEIGHT/2-8, 
                                bn::to_string<32>(fish_points), text_sprites);

        update_camera_check_edge(camera, pj, lvl0); //warning put just before bn::core:update()

        if (current_life>0) current_life = current_life - decreaze_value; else {/*DEATH*/ }
        lifebar.set_tiles(bn::sprite_items::spr_lifebar.tiles_item().create_tiles(current_life.round_integer())); //Lifebar update

        bn::core::update();
    }
}
