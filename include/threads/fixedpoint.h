#ifndef THREADS_FIXEDPOINT_H
#define THREADS_FIXEDPOINT_H


#define F (1<<14)
#define INT_TO_FIXED_POINT(n) ((n)*(F))
#define FIXED_POINT_TO_INT(x) ((x)/(F))

#define FIXED_POINT_TO_INT_NEAREST(x) (((x) >= 0) ? (((x) + (F / 2)) / (F)) : (((x) - (F / 2)) / (F)))
#define X_ADD_Y(x, y) ((x) + (y))
#define X_SUBTRACT_Y(x, y) ((x) - (y))
#define X_ADD_N(x, n) ((x) + ((n)*(F)))
#define X_SUBTRACT_N(x, n) ((x) - (n)*(F))
#define X_MULTIPLY_Y(x, y) (((int64_t)(x)) * (y)/F)
#define X_MULTIPLY_N(x, n) ((x) * (n))
#define X_DIVIDE_Y(x, y) (((int64_t)(x)) * F / (y))
#define X_DIVIDE_N(x, n) ((x) / (n))



#endif /* threads/thread.h */