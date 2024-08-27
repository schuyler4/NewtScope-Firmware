#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>

#define SIGNAL_LENGTH 75
#define BUFFER_LENGTH 10

typedef struct
{
    uint8_t* buffer;
    uint8_t buffer_index;
} RingBuffer;

void ring_buffer_init(RingBuffer* ring_buffer_pointer)
{
    ring_buffer_pointer->buffer = malloc(BUFFER_LENGTH);
    ring_buffer_pointer->buffer_index = 0;
}

void ring_buffer_sample(uint8_t point, RingBuffer* ring_buffer_pointer)
{
    *(ring_buffer_pointer->buffer + ring_buffer_pointer->buffer_index) = point;
    ring_buffer_pointer->buffer_index++;
    if(ring_buffer_pointer->buffer_index >= BUFFER_LENGTH)
    {
        ring_buffer_pointer->buffer_index = 0;
    }
}

void flatten_ring_buffer(RingBuffer* ring_buffer_pointer, uint8_t* flat_buffer)
{
    uint8_t i;
    for(i = 0; i < BUFFER_LENGTH; i++)
    {
        if(i < ring_buffer_pointer->buffer_index)
        {
            flat_buffer[BUFFER_LENGTH - ring_buffer_pointer->buffer_index + i] = ring_buffer_pointer->buffer[i];  
        }
        else
        {
            flat_buffer[i-ring_buffer_pointer->buffer_index] = ring_buffer_pointer->buffer[i];
        }
        /*
        flat_buffer[i+(BUFFER_LENGTH-ring_buffer_pointer->buffer_index)] = ring_buffer_pointer->hold_buffer[i];
        if(i < ring_buffer_pointer->buffer_index) 
            flat_buffer[2*BUFFER_LENGTH-ring_buffer_pointer->buffer_index+i] = ring_buffer_pointer->current_buffer[i];
        else
            flat_buffer[i-ring_buffer_pointer->buffer_index] = ring_buffer_pointer->current_buffer[i];
        */

    }
}

void ring_buffer_teardown(RingBuffer* ring_buffer_pointer)
{
    free(ring_buffer_pointer->buffer);
}

void print_array(uint8_t* array, uint8_t length)
{
    uint8_t i;
    for(i = 0; i < length; i++) printf("%d ", *(array + i));
    printf("\n");
}

int main(void)
{
    RingBuffer ring_buffer;
    ring_buffer_init(&ring_buffer);
    uint8_t i;
    for(i = 1; i <= SIGNAL_LENGTH; i++)
    {
        ring_buffer_sample(i, &ring_buffer);
        uint8_t flat_buffer[BUFFER_LENGTH]; 
        flatten_ring_buffer(&ring_buffer, flat_buffer);
        print_array(flat_buffer, BUFFER_LENGTH);
    }
    ring_buffer_teardown(&ring_buffer);
    return 0;
}
