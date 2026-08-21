#ifndef LAMBDA_MONITOR_CONFIG_H
#define LAMBDA_MONITOR_CONFIG_H

/* ADC1 channel wired to the lambda sensor's op-amp output.
 * On the XIAO ESP32-S3, ADC1 channel N = GPIO(N+1), so channel 1 = GPIO2 (D1/A1).
 * KONZEPT.md recommends GPIO1/A0/D0 (channel 0) as the least contested pin;
 * change this if rewiring to that pin. Must stay an ADC1-capable pin (GPIO1-10). */
#define ADC_CHANNEL_LAMBDA 1

/* ADC1 attenuated full-scale range (ADC_ATTEN_DB_12, see adc_task.c). */
#define ADC_MV_MIN 0
#define ADC_MV_MAX 3300

/* Window length for the rolling control-quality average (index_avg_2s):
 * a symmetrically-dithering step-lambda closed loop should average out to
 * ~0 (stoichiometric) over this window even though instantaneous readings
 * swing lean/rich; a sustained non-zero average indicates a real mixture
 * bias rather than normal dither. Must cover at least one full switching
 * cycle to be meaningful. */
#define AVG_WINDOW_S 2

/* Wi-Fi access point credentials. */
#define WIFI_AP_SSID "lambda-monitor"
#define WIFI_AP_PASSWORD "lambda1234"

/* Default sensor calibration (Bosch step lambda sensor, 3.2x op-amp gain).
 * u_min/u_max/u_lambda1 are in mV; thresholds are mixture indices [-100, 100]. */
#define CALIB_DEFAULT_U_MIN_MV          320  /* 0.1V raw x 3.2 gain (lean extreme) */
#define CALIB_DEFAULT_U_MAX_MV         2880  /* 0.9V raw x 3.2 gain (rich extreme) */
#define CALIB_DEFAULT_U_LAMBDA1_MV     1600  /* stoichiometric midpoint */
#define CALIB_DEFAULT_DEADBAND_MV       160  /* ~5.6% of range, hysteresis tolerance */
#define CALIB_DEFAULT_THRESH_VERY_LEAN  -60
#define CALIB_DEFAULT_THRESH_LEAN       -20
#define CALIB_DEFAULT_THRESH_RICH        20
#define CALIB_DEFAULT_THRESH_VERY_RICH   60

#endif /* LAMBDA_MONITOR_CONFIG_H */
