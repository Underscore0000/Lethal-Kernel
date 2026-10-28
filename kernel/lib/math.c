/*
 * Lethal Kernel - minimal libm subset for stb_truetype
 */
#include <stdint.h>

double fabs(double x) { return x < 0.0 ? -x : x; }

double floor(double x) {
    if (x >= 0.0) return (double)(long long)x;
    long long i = (long long)x;
    return (double)(x == (double)i ? i : i - 1);
}

double ceil(double x) { return -floor(-x); }

double sqrt(double x) {
    if (x <= 0.0) return 0.0;
    double r = x;
    for (int i = 0; i < 24; i++)
        r = 0.5 * (r + x / r);
    return r;
}

double fmod(double x, double y) {
    if (y == 0.0) return 0.0;
    long long q = (long long)(x / y);
    return x - (double)q * y;
}

double pow(double x, double y) {

    if (y == 0.0) return 1.0;
    if (y == 1.0) return x;
    if (y == 2.0) return x * x;
    if (y == 0.5) return sqrt(x);
    if (y < 0.0) { y = -y; x = 1.0 / x; }
    int n = (int)y;
    double frac = y - (double)n;
    double r = 1.0;
    for (int i = 0; i < n; i++) r *= x;
    if (frac > 0.0) r *= (1.0 + frac * (x - 1.0)); 
    return r;
}

double cos(double x) {

    double x2 = x * x;
    double r = 1.0;
    double term = 1.0;
    for (int i = 1; i < 12; i++) {
        term *= -x2 / (double)((2*i-1) * (2*i));
        r += term;
    }
    return r;
}

double acos(double x) {

    if (x >= 1.0) return 0.0;
    if (x <= -1.0) return 3.14159265358979323846;

    double x2 = x * x;
    double term = x;
    double sum = x;
    for (int n = 1; n < 10; n++) {
        term *= x2 * (double)(2*n - 1) / (double)(2*n);
        sum += term / (double)(2*n + 1);
    }
    return 1.57079632679489661923 - sum;
}