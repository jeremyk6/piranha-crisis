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
#define PJ_STATE_STANDING   0
#define PJ_STATE_EATING     1
#define PJ_STATE_SWALLOWING 2
#define PJ_INVINCIBLE_DELAY 30
#define PJ_EATING_ANIMATION_DELAY 100
#define CAMERA_NORMAL 0
#define CAMERA_RUMBLE 1
#define LIFE_DECREASE_VALUE 0.2/60

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
#define FISH_BOXSIZE 12

#define FISH_STATE_APPEARING    0
#define FISH_STATE_NORMAL       1
#define FISH_STATE_DYING        2
#define FISH_STATE_DEAD         3

#define DIRECTION_UP            0
#define DIRECTION_UP_RIGHT      1
#define DIRECTION_RIGHT         2
#define DIRECTION_DOWN_RIGHT    3
#define DIRECTION_DOWN          4
#define DIRECTION_DOWN_LEFT     5
#define DIRECTION_LEFT          6
#define DIRECTION_UP_LEFT       7
#define DIRECTION_NONE          8

#define PJ_ANIMATION_STAND  0
#define PJ_ANIMATION_EAT    1


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

bn::fixed lvl0_spikes(bn::fixed x, bn::fixed y, bn::regular_bg_map_item map_item)
{
    return (bg_collision_tile_at(x, y, map_item, 5)==1 ||
            bg_collision_tile_at(x, y, map_item, 6)==1 ||
            bg_collision_tile_at(x, y, map_item, 10)==1 ||
            bg_collision_tile_at(x, y, map_item, 3077)==1 ||
            bg_collision_tile_at(x, y, map_item, 3078)==1);
}


void update_camera_check_edge(bn::camera_ptr camera, bn::fixed x, bn::fixed y, bn::regular_bg_ptr bg)
{
    if ((x.round_integer()+bg.dimensions().width()/2) < GBA_SCREEN_WIDTH/2+CAM_OFFSET_LEFT_LIMIT) //LEFT CORNER
    {
        camera.set_x(GBA_SCREEN_WIDTH/2-bg.dimensions().width()/2+CAM_OFFSET_LEFT_LIMIT);
    }
    if ((x.round_integer()+bg.dimensions().width()/2) > bg.dimensions().width()-GBA_SCREEN_WIDTH/2-CAM_OFFSET_RIGHT_LIMIT) //RIGHT CORNER
    {
        camera.set_x((bg.dimensions().width()-GBA_SCREEN_WIDTH/2)-bg.dimensions().width()/2-CAM_OFFSET_RIGHT_LIMIT);
    }

    if ((y.round_integer()+bg.dimensions().height()/2) < GBA_SCREEN_HEIGHT/2+CAM_OFFSET_UP_LIMIT) //UP CORNER
    {
        camera.set_y(GBA_SCREEN_HEIGHT/2-bg.dimensions().height()/2+CAM_OFFSET_UP_LIMIT);
    }
    if ((y.round_integer()+bg.dimensions().height()/2) > bg.dimensions().height()-GBA_SCREEN_HEIGHT/2-CAM_OFFSET_DOWN_LIMIT) //DOWN CORNER
    {
        camera.set_y((bg.dimensions().height()-GBA_SCREEN_HEIGHT/2)-bg.dimensions().height()/2-CAM_OFFSET_DOWN_LIMIT);
    } 
}

class Fish {
    protected:
        bn::optional<bn::sprite_ptr> sprite;
        bn::optional<bn::sprite_animate_action<4>> animation;
        bn::camera_ptr* camera;
        bn::fixed speed;
        bn::fixed x;
        bn::fixed y;
        bn::random* random;
        bn::regular_bg_ptr* bg;
        bn::regular_bg_map_item* bg_map_item;
        //unsigned char type;
        unsigned char timer_appearing;
        unsigned char timer_dying;
        unsigned short timer_init;
        unsigned short timer_wait;
        unsigned short timer;
        unsigned char direction;
        char state;
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
            if(this->state == FISH_STATE_APPEARING) {
                this->timer_appearing-=1;
                this->sprite->set_visible(!this->sprite->visible());
                if(this->timer_appearing==0) {
                    this->sprite->set_visible(true);
                    this->state = FISH_STATE_NORMAL;
                }

            }
            if(this->state == FISH_STATE_DYING) {
                this->timer_dying -= 1;
                this->sprite->set_visible(!this->sprite->visible());
                if(this->timer_dying == 0) {
                    this->state = FISH_STATE_DEAD;
                }
            }
            if(this->state == FISH_STATE_NORMAL)
            {
                if(direction == DIRECTION_UP) {
                    this->y -= speed;
                    if (this->y < -this->bg->dimensions().height()/2+CAM_OFFSET_UP_LIMIT) direction = DIRECTION_DOWN;
                }
                if(direction == DIRECTION_UP_RIGHT) {
                    this->x += speed;
                    this->y -= speed;
                    this->sprite->set_horizontal_flip(true);
                    if (this->y < -this->bg->dimensions().height()/2+CAM_OFFSET_UP_LIMIT) direction = DIRECTION_DOWN_RIGHT;
                    if (this->x > this->bg->dimensions().width()/2-CAM_OFFSET_RIGHT_LIMIT) direction = DIRECTION_UP_LEFT;
                }
                if(direction == DIRECTION_RIGHT) {
                    this->x += speed;
                    this->sprite->set_horizontal_flip(true);
                    if (this->x > this->bg->dimensions().width()/2-CAM_OFFSET_RIGHT_LIMIT) direction = DIRECTION_LEFT;
                }
                if(direction == DIRECTION_DOWN_RIGHT) {
                    this->x += speed;
                    this->y += speed;
                    this->sprite->set_horizontal_flip(true);
                    if (this->y > this->bg->dimensions().height()/2-CAM_OFFSET_DOWN_LIMIT) direction = DIRECTION_UP_RIGHT;
                    if (this->x > this->bg->dimensions().width()/2-CAM_OFFSET_RIGHT_LIMIT) direction = DIRECTION_DOWN_LEFT;
                }
                if(direction == DIRECTION_DOWN) {
                    this->y += speed;
                    if (this->y > this->bg->dimensions().height()/2-CAM_OFFSET_DOWN_LIMIT) direction = DIRECTION_UP;
                }
                if(direction == DIRECTION_DOWN_LEFT) {
                    this->x -= speed;
                    this->y += speed;
                    this->sprite->set_horizontal_flip(false);
                    if (this->y > this->bg->dimensions().height()/2-CAM_OFFSET_DOWN_LIMIT) direction = DIRECTION_UP_LEFT;
                    if (this->x < -this->bg->dimensions().width()/2+CAM_OFFSET_LEFT_LIMIT) direction = DIRECTION_DOWN_RIGHT;
                }
                if(direction == DIRECTION_LEFT) {
                    this->x -= speed;
                    this->sprite->set_horizontal_flip(false);
                    if (this->x < -this->bg->dimensions().width()/2+CAM_OFFSET_LEFT_LIMIT) direction = DIRECTION_RIGHT;
                }
                if(direction == DIRECTION_UP_LEFT) {
                    this->x -= speed;
                    this->y -= speed;
                    this->sprite->set_horizontal_flip(false);
                    if (this->y < -this->bg->dimensions().height()/2+CAM_OFFSET_UP_LIMIT) direction = DIRECTION_DOWN_LEFT;
                    if (this->x < -this->bg->dimensions().width()/2+CAM_OFFSET_LEFT_LIMIT) direction = DIRECTION_UP_RIGHT;
                }

                if(direction != DIRECTION_NONE) this->animation->update();
                
                // Gestion du timing
                this->timer -= 1;
                if(this->timer == 0) {
                    bool collision = (lvl0_collisions(this->x, this->y, *this->bg_map_item)==1 || lvl0_spikes(this->x, this->y, *this->bg_map_item)==1);
                    if(direction == DIRECTION_NONE || this->timer_wait == 0 || collision) {
                        if (!collision) direction = random->get_int(8);
                        timer = this->timer_init;
                    } else {
                        direction = DIRECTION_NONE;
                        timer = this->timer_wait;
                    }
                }
            }
            // Déplacement du sprite
            this->sprite->set_x(this->x - this->camera->x());
            this->sprite->set_y(this->y - this->camera->y());
        }
        char getState() {
            return(this->state);
        }
        void kill() {
            this->state = FISH_STATE_DYING;
        }
        Fish(bn::fixed init_x, bn::fixed init_y, bn::camera_ptr& cam, bn::random& rand, bn::fixed speed_value, unsigned short timer_value, unsigned short timer_wait_value, bn::regular_bg_ptr& bg_item, bn::regular_bg_map_item& map) {
            this->x = init_x;
            this->y = init_y;
            this->camera = &cam;
            this->bg = &bg_item;
            this->bg_map_item = &map;
            this->random = &rand;
            this->speed = speed_value;
            this->timer_init = timer_value;
            this->timer_wait = timer_wait_value;
            this->timer = timer_value;
            this->direction = random->get_int(8);
            this->state = FISH_STATE_APPEARING;
            this->timer_appearing = 30;
            this->timer_dying = 30;
        }
};

class NormalFish : public Fish {
    public : 
        NormalFish(bn::fixed init_x, bn::fixed init_y, bn::camera_ptr& cam, bn::random& rand, bn::regular_bg_ptr& bg_item, bn::regular_bg_map_item& map) : Fish(init_x, init_y, cam, rand, rand.get_fixed(0.5,1), rand.get_int(50,100), rand.get_int(50,100), bg_item, map) {
            this->sprite = bn::sprite_items::fish_normal.create_sprite(init_x, init_y);
            this->animation = bn::create_sprite_animate_action_forever(*this->sprite, 4, bn::sprite_items::fish_normal.tiles_item(), 0, 1, 2, 1);
        }
        unsigned char getType() {
            return(FISH_TYPE_NORMAL);
        }
};


class SpeedFish : public Fish {
    public : 
        SpeedFish(bn::fixed init_x, bn::fixed init_y, bn::camera_ptr& cam, bn::random& rand, bn::regular_bg_ptr& bg_item, bn::regular_bg_map_item& map) : Fish(init_x, init_y, cam, rand, rand.get_fixed(1.5,2), rand.get_int(50,100), rand.get_int(50,100), bg_item, map) {
            this->sprite = bn::sprite_items::fish_speed.create_sprite(init_x, init_y);
            this->animation = bn::create_sprite_animate_action_forever(*this->sprite, 4, bn::sprite_items::fish_speed.tiles_item(), 0, 1, 2, 1);
        }
        unsigned char getType() {
            return(FISH_TYPE_SPEED);
        }
};

class Player {
    private :
        bn::optional<bn::sprite_ptr> sprite;
        bn::optional<bn::sprite_animate_action<4>> animation;
        bn::camera_ptr* camera;
        char* camera_state;
        bn::regular_bg_map_item* map_item;
        bn::fixed speed_x;
        bn::fixed speed_y;
        bn::fixed acceleration;
        bn::fixed maxspeed;
        char state;
        short eating_timer;
        short swallowing_timer;
        bool eaten;
        bn::fixed life;
        bool is_hurt = false;
        short hurt_timer;

    public :
        Player(bn::fixed x, bn::fixed y, bn::camera_ptr& cam, char& cam_state, bn::regular_bg_map_item& bg_map_item) {
            this->sprite = bn::sprite_items::pj.create_sprite(x, y);
            this->state = PJ_ANIMATION_STAND;
            this->camera = &cam;
            this->sprite->set_camera(cam);
            this->camera_state = &cam_state;
            this->map_item = &bg_map_item;
            this->speed_x = 0;
            this->speed_y = 0;
            this->acceleration = 0.03;
            this->life = 8;
            this->setStateStand();
            this->hurt_timer = 0;
        }
        short getLife() {
            return(this->life.round_integer());
        }
        void eat() {
            this->speed_y+= 0.5;
            this->eaten = true;
            this->life = this->life.round_integer() + 1;
            if(this->life > 8) this->life = 8;
            bn::sound_items::eating.play(1);
            this->animation = bn::create_sprite_animate_action_once(*this->sprite, 8, bn::sprite_items::pj.tiles_item(), 8, 4);
        }
        void hurt() {
            *this->camera_state = CAMERA_RUMBLE;
            bn::sound_items::spike.play(1);
            if(this->is_hurt == false) {
                this->life = this->life.round_integer() - 1;
                this->is_hurt=true;
            }
        }
        bn::fixed x() {
            return(this->sprite->x());
        }
        bn::fixed y() {
            return(this->sprite->y());
        }
        char getState() {
            return(this->state);
        }
        void setStateSwallowing() {
            this->state = PJ_STATE_SWALLOWING;
            this->maxspeed = this->acceleration*20;
            this->animation = bn::create_sprite_animate_action_forever(*this->sprite, 32, bn::sprite_items::pj.tiles_item(), 8, 9, 10, 11);
        }
        void setStateStand() {
            this->eaten = false;
            this->swallowing_timer = 0;
            this->state = PJ_STATE_STANDING;
            this->maxspeed = this->acceleration*50;
            this->eating_timer = 0;
            this->animation = bn::create_sprite_animate_action_forever(*this->sprite, 32, bn::sprite_items::pj.tiles_item(), 0, 1, 2, 3);
        }
        void setStateEat() {
            this->state = PJ_STATE_EATING;
            this->maxspeed = this->acceleration*80;
            this->animation = bn::create_sprite_animate_action_forever(*this->sprite, 32, bn::sprite_items::pj.tiles_item(), 4, 5, 6, 7);
            bn::sound_items::grunting.play(1);
        }
        
        void update() {
            bool left_collision = (lvl0_collisions(this->sprite->x()+this->speed_x, this->sprite->y(), *this->map_item)==1);
            bool right_collision = (lvl0_collisions(this->sprite->x()+this->speed_x, this->sprite->y(), *this->map_item)==1);
            bool up_collision = (lvl0_collisions(this->sprite->x(), this->sprite->y()+this->speed_y, *this->map_item)==1);
            bool down_collision = (lvl0_collisions(this->sprite->x(), this->sprite->y()+this->speed_y, *this->map_item)==1);

            if(bn::keypad::left_held() && !left_collision) {
                if(this->speed_x >= -this->maxspeed) this->speed_x -= this->acceleration;
                if(this->speed_x <= -this->maxspeed) this->speed_x += this->acceleration;
                if(this->speed_x < 0) this->sprite->set_horizontal_flip(true);
            } 
            else if(bn::keypad::right_held() && !right_collision) {
                if(this->speed_x <= this->maxspeed) this->speed_x += this->acceleration;
                if(this->speed_x >= this->maxspeed) this->speed_x -= this->acceleration;
                if(this->speed_x > 0) this->sprite->set_horizontal_flip(false);
            }
            else {
                if(this->speed_x < 0 && !left_collision)
                    this->speed_x += this->acceleration*1.5;
                if(this->speed_x > 0 && !right_collision)
                    this->speed_x -= this->acceleration*1.5;
            }
            if(bn::keypad::up_held() && !up_collision) {
                if(this->speed_y >= -this->maxspeed) this->speed_y -= this->acceleration;
                if(this->speed_y <= -this->maxspeed) this->speed_y += this->acceleration;
            }
            else if(bn::keypad::down_held() && !down_collision) {
                if(this->speed_y <= this->maxspeed) this->speed_y += this->acceleration;
                if(this->speed_y >= this->maxspeed) this->speed_y -= this->acceleration;
            }
            else {
                if(this->speed_y < 0 && !down_collision) 
                    this->speed_y += this->acceleration*1.5;

                if(this->speed_y > 0 && !up_collision) 
                    this->speed_y -= this->acceleration*1.5;
            }

            if(left_collision || right_collision) {
                this->speed_x = -this->speed_x;
                if(this->state == PJ_STATE_EATING) {
                    *this->camera_state = CAMERA_RUMBLE;
                    bn::sound_items::choc.play(abs(this->speed_x/this->maxspeed) > 1 ? 1 : abs(this->speed_x/this->maxspeed));
                }
                else{
                    bn::sound_items::boing.play(1);
                }
            }
            if(up_collision || down_collision) {
                this->speed_y = -this->speed_y;
                if(this->state == PJ_STATE_EATING) {
                    *this->camera_state = CAMERA_RUMBLE;
                    bn::sound_items::choc.play(abs(this->speed_y/this->maxspeed) > 1 ? 1 : abs(this->speed_y/this->maxspeed));
                }
                else{
                    bn::sound_items::boing.play(1);
                }
            }

            //Spikes collision
            if(bg_collision_tile_at(this->sprite->x(), this->sprite->y(), *this->map_item, 3077)==1) {
                this->speed_x = -this->maxspeed; //left_spikes
                this->hurt();
            }
            if(bg_collision_tile_at(this->sprite->x(), this->sprite->y(), *this->map_item, 5)==1) {
                this->speed_x = this->maxspeed; //right_spikes
                this->hurt();
            }
            if(bg_collision_tile_at(this->sprite->x(), this->sprite->y(), *this->map_item, 6)==1) {
                this->speed_y = -this->maxspeed; //up_spikes
                this->hurt();
            }
            if(bg_collision_tile_at(this->sprite->x(), this->sprite->y(), *this->map_item, 10)==1) {
                this->speed_y = -this->maxspeed; //up_spikes (WTF)
                this->hurt();
            }
            if(bg_collision_tile_at(this->sprite->x(), this->sprite->y(), *this->map_item, 3078)==1) {
                this->speed_y = this->maxspeed; //down_spikes
                this->hurt();
            }

            this->sprite->set_x(this->sprite->x() + this->speed_x);
            this->camera->set_x(this->camera->x() + this->speed_x);
            this->sprite->set_y(this->sprite->y() + this->speed_y);
            this->camera->set_y(this->camera->y() + this->speed_y);

            if(this->state == PJ_STATE_SWALLOWING) {
                this->swallowing_timer += 1;
                if(this->swallowing_timer >= PJ_EATING_ANIMATION_DELAY) {
                    this->setStateStand();
                }
            }

            if(this->state == PJ_STATE_EATING) {
                if(this->animation->done())
                    this->animation = bn::create_sprite_animate_action_forever(*this->sprite, 32, bn::sprite_items::pj.tiles_item(), 4, 5, 6, 7);
                this->eating_timer += 1;
                if(this->eating_timer >= PJ_EATING_ANIMATION_DELAY) {
                    if(this->eaten == true)
                        this->setStateSwallowing();
                    else
                        this->setStateStand();
                }
            }

            // Invincible si trop de collisions simultanées
            if(this->is_hurt) {
                this->hurt_timer++;
                if(this->hurt_timer >= PJ_INVINCIBLE_DELAY) {
                    this->is_hurt = false;
                    this->hurt_timer = 0;
                }
            }

            this->animation->update();

            if (this->life>0) this->life -= LIFE_DECREASE_VALUE;
            else this->life = 0;
        }
};

// cam.x() - screen_width/2 cam.x() + screen_width/2

Fish createFish(bn::camera_ptr& cam, bn::random& rand, bn::regular_bg_ptr& bg_item, bn::regular_bg_map_item& map) {
    int type = rand.get_int(2);
    bn::fixed x = rand.get_int(bg_item.dimensions().width())-bg_item.dimensions().width()/2;
    bn::fixed y = rand.get_int(bg_item.dimensions().height())-bg_item.dimensions().height()/2;
    switch(type) {
        case 0:
            return(NormalFish(x, y, cam, rand, bg_item, map));
            break;
        case 1:
            return(SpeedFish(x, y, cam, rand, bg_item, map));
            break;
        default:
            return(NormalFish(x, y, cam, rand, bg_item, map));
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
    bn::sprite_ptr lifebar = bn::sprite_items::spr_lifebar.create_sprite(16-GBA_SCREEN_WIDTH/2, 16-GBA_SCREEN_HEIGHT/2);
    bn::sprite_ptr counter = bn::sprite_items::spr_counter.create_sprite(GBA_SCREEN_WIDTH/2-16, GBA_SCREEN_HEIGHT/2-16);

    bn::sprite_text_generator text_generator(common::variable_8x16_sprite_font);
    text_generator.set_center_alignment();
    bn::vector<bn::sprite_ptr, 32> text_sprites;

    /* Random generator */
    bn::random random = bn::random();

    /*
        Scoring
    */
    int fish_points = 0;
    
    /*
        Camera
    */
    unsigned char camera_rumble_index = 0;
    short rumble_amplitude = 3;
    bn::fixed camera_rumble[] = {-rumble_amplitude, rumble_amplitude, rumble_amplitude, -rumble_amplitude, 
                                 -rumble_amplitude, rumble_amplitude, rumble_amplitude, -rumble_amplitude,
                                 -rumble_amplitude, rumble_amplitude, rumble_amplitude, -rumble_amplitude,
                                 -rumble_amplitude, rumble_amplitude, rumble_amplitude, -rumble_amplitude};
    char camera_state = CAMERA_NORMAL;

    //pj.set_camera(camera);
    lvl0.set_camera(camera);

    /*
        Musique BG
    */
    bn::music_items::music.play(0.5);

    #define FISH_NUMBER 50
    bn::vector<Fish, FISH_NUMBER> fish_list;
    for(char i = 0; i < FISH_NUMBER; i++) {
        fish_list.push_back(createFish(camera, random, lvl0, lvl0_map_item));
    }

    Player player = Player(0, 0, camera, camera_state, lvl0_map_item);

    while(true)
    {
        
        for(char fish_index = 0; fish_index < fish_list.size(); fish_index++) {
            fish_list.at(fish_index).update();
            if(fish_list.at(fish_index).collision(player.x(), player.y()) && player.getState() == PJ_STATE_EATING && fish_list.at(fish_index).getState() == FISH_STATE_NORMAL) {
                // Quand on avale un poisson
                fish_list.at(fish_index).kill();
                player.eat();
                fish_points+=1;
            }
            if(fish_list.at(fish_index).getState() == FISH_STATE_DEAD) {
                fish_list.erase(&fish_list.at(fish_index));
                fish_list.push_back(createFish(camera, random, lvl0, lvl0_map_item));
            }
        }

        // Passage en mode eat
        if(bn::keypad::a_pressed() && player.getState() == PJ_STATE_STANDING) {
            player.setStateEat();
        }

        player.update();

        update_affine_background(base_degrees_angle, attributes, attributes_hbe);
        
        text_sprites.clear();
        //text_generator.generate(0, -40, bn::to_string<32>(get_bgtile_at_pos(pj.x(),pj.y(),lvl0_map_item)), text_sprites);
        //text_generator.generate(0, 40, bn::to_string<32>(fish_list[0]->getX()), text_sprites);
        //text_generator.generate(0, -70, bn::to_string<32>(collision), text_sprites);
        //text_generator.generate(-110, -70, bn::to_string<32>(pj.x().integer() + 8*lvl0_map_item.dimensions().width()/2), text_sprites);
        

        text_generator.generate(GBA_SCREEN_WIDTH/2-8, GBA_SCREEN_HEIGHT/2-8, 
                                bn::to_string<32>(fish_points), text_sprites);

        update_camera_check_edge(camera, player.x(), player.y(), lvl0); //warning put just before bn::core:update()
        if(camera_state == CAMERA_RUMBLE){
            camera.set_position(camera.x()+camera_rumble[camera_rumble_index], camera.y()+camera_rumble[camera_rumble_index]);
            camera_rumble_index++;
            if(camera_rumble_index > (sizeof(camera_rumble) / sizeof(bn::fixed))-1) {
                camera_state = CAMERA_NORMAL;
                camera_rumble_index = 0;
            }
        }

        lifebar.set_tiles(bn::sprite_items::spr_lifebar.tiles_item().create_tiles(player.getLife())); //Lifebar update

        if(player.getLife() == 0) {
            // GAME OVER
        }

        bn::core::update();
    }
}
