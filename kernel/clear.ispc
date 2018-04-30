task void fast_clear_task(uniform int buffer[], uniform int width, uniform int height, uniform int xspan, uniform int yspan, uniform int color)
{
    const uniform int xstart = taskIndex0 * xspan;
    const uniform int xend = min(xstart + xspan, width);
    const uniform int ystart = taskIndex1 * yspan;
    const uniform int yend = min(ystart + yspan, height);

    foreach (y = ystart ... yend, x = xstart ... xend) {
        int index = y * width + x;
        buffer[index] = color;
    }
}


export void fast_clear(uniform int buffer[], uniform int width, uniform int height, uniform int color)
{
    const uniform int xspan = max(32, programCount*2);
    const uniform int yspan = 16; 

    launch[width/xspan, height/yspan] fast_clear_task(buffer, width, height, xspan, yspan, color);
    sync;
}
