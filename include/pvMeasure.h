#ifndef PV_MEASURE_H
#define PV_MEASURE_H

/**
 * Objekt pre meranie meteostanice
 *
 * Posledná zmena(last change): 30.07.2022
 * @author Ing. Peter VOJTECH ml. <petak23@gmail.com>
 * @copyright  Copyright (c) 2022 - 2022 Ing. Peter VOJTECH ml.
 * @license
 * @link       http://petak23.echo-msz.eu
 * @version 1.0.0
 *
 */
class pvMeasure
{
public:
  // pvMeasure();

  float temperature;
  float humidity;
  float rel_pressure;
  float abs_pressure;
  long time;
  int priority;
};

#endif