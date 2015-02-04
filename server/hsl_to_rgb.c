#include "hsl_to_rgb.h"

void HSL_to_RGB(double H, double S, double L, uint8_t *R, uint8_t *G, uint8_t *B) {
  double var_1, var_2;
  if ( S == 0.0 )                       //HSL from 0 to 1
  {
     *R = (uint8_t)(L * 255.0 + 0.5);
     *G = (uint8_t)(L * 255.0 + 0.5);
     *B = (uint8_t)(L * 255.0 + 0.5);
  }
  else
  {
     if ( L < 0.5 )
       var_2 = L * ( 1 + S );
     else
       var_2 = ( L + S ) - ( S * L );

     var_1 = 2.0 * L - var_2;

     (*R) = (uint8_t)(255.0 * Hue_2_RGB( var_1, var_2, H + ( 1.0 / 3.0 ) ));
     (*G) = (uint8_t)(255.0 * Hue_2_RGB( var_1, var_2, H ));
     (*B) = (uint8_t)(255.0 * Hue_2_RGB( var_1, var_2, H - ( 1.0 / 3.0 ) ));
  }
}

double Hue_2_RGB(double v1, double v2, double vH )             //Function Hue_2_RGB
{
   if ( vH < 0.0 ) vH += 1.0;
   if ( vH > 1.0 ) vH -= 1.0;
   if ( ( 6.0 * vH ) < 1 ) return ( v1 + ( v2 - v1 ) * 6.0 * vH );
   if ( ( 2.0 * vH ) < 1 ) return ( v2 );
   if ( ( 3.0 * vH ) < 2 ) return ( v1 + ( v2 - v1 ) * ( ( 2.0 / 3.0 ) - vH ) * 6.0 );
   return ( v1 );
}
