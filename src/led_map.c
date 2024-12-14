#include <stdlib.h>
#include <stdio.h>
#include "led_map.h"

typedef struct {
	int size;
	int dimensions;
	int32_t coord[];
} led_map_t;

static led_map_t *map=NULL;

void led_map_create(int size, int dimensions) {
	free(map);
	map=calloc(sizeof(led_map_t)+sizeof(int32_t)*size*dimensions, 1);
	map->size=size;
	map->dimensions=dimensions;
}

void led_map_set_led_pos(int led, float x, float y, float z) {
	if (!map || led<0 || led>=map->size) return;
	int p=led*map->dimensions;
	map->coord[p++]=x*65536;
	if (map->dimensions>1) map->coord[p++]=y*65536;
	if (map->dimensions>2) map->coord[p++]=z*65536;
}

int led_map_get_led_pos(int led, float *x, float *y, float *z) {
	if (!map || led<0 || led>=map->size) return 0;
	int p=led*map->dimensions;
	*x=map->coord[p++]/65536.0;
	if (map->dimensions>1) *y=map->coord[p++]/65536.0;
	if (map->dimensions>2) *z=map->coord[p++]/65536.0;
	return 1;
}


int led_map_get_led_pos_fixed(int led, int32_t *pos) {
	if (!map || led<0 || led>=map->size) return 0;
	int p=led*map->dimensions;
	pos[0]=map->coord[p++];
	if (map->dimensions>1) pos[1]=map->coord[p++];
	if (map->dimensions>2) pos[2]=map->coord[p++];
	return 1;
}


int led_map_get_size() {
	return map->size;
}

int led_map_get_dimensions() {
	return map->dimensions;
}

void *led_map_get_blob(int *len_bytes) {
	if (map) *len_bytes=sizeof(led_map_t)+sizeof(int32_t)*map->size*map->dimensions;
	return map;
}


void led_map_set_blob(void *data, int len_bytes) {
	free(map);
	map=data;
	int check_len_bytes=sizeof(led_map_t)+sizeof(int32_t)*map->size*map->dimensions;
	if (len_bytes!=check_len_bytes) {
		printf("led_map_set_blob: Aiee! passed %d bytes but map should be %d bytes\n", len_bytes, check_len_bytes);
	}
}
