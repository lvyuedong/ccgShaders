/**************************************************
Source code originally comes from ccg_mix20layers.cpp by Jan Sandstrom (Pixero)
modified by lvyuedong
***************************************************/

#include <math.h>
#include "shader.h"

#pragma warning(disable : 4267)
//#pragma warning (disable : 4244)

struct ccg_mix20layers {
  //global attributes
  miInteger g_layers_number;
  miBoolean g_inverse_alpha;
  miBoolean g_enable_cc;
  miScalar  g_hue;
  miScalar  g_saturation;
  miScalar  g_value;
  miScalar  g_gamma;
  miScalar  g_contrast;
  miScalar  g_brightness;

  //local attributes
  miBoolean Enable[20];
  miInteger BlendMode[20];
  miColor   Opacity[20];
  miColor   Layer[20];
  miInteger AlphaMode[20];

  miScalar  mix[20];
  miBoolean alphaInvert[20];
  miBoolean colorInvert[20];
  miBoolean useOpacity[20];
  miBoolean enable_cc[20];
  miScalar  hue[20];
  miScalar  saturation[20];
  miScalar  value[20];
  miScalar  gamma[20];

  //clip switch
  miBoolean ColorClip;
};

extern "C" DLLEXPORT int ccg_mix20layers_version() { return 1;}

void ccg_mix20layers_rgb2hsv(miColor i, miColor *o)
{
  float min, max, delta;

  //be sure there is no negative channel
  i.r = i.r<0?0:i.r;
  i.g = i.g<0?0:i.g;
  i.b = i.b<0?0:i.b;
 
  o->a = i.a;
 
  //find min and max of components
  min = i.r < i.g ? (i.r < i.b ? i.r : i.b) : (i.g < i.b ? i.g : i.b);
  max = i.r > i.g ? (i.r > i.b ? i.r : i.b) : (i.g > i.b ? i.g : i.b);
  delta = max - min;
   
  //value
  o->b = max;

  //saturation
  if(max!=0.0f) o->g = delta / max;
  else {
      o->g = 0.0f;
      o->r = -1.0f;
      return;
     }
    
  //hue
  if(max==min) o->r = 0.0f;
  else if(max==i.r) o->r = fmod((60.0f*(i.g - i.b)/delta+360.0f), 360.0f);
  else if(max==i.g) o->r = 60.0f*(i.b - i.r)/delta + 120.0f;
  else if(max==i.b) o->r = 60.0f*(i.r - i.g)/delta + 240.0f;
}

void ccg_mix20layers_hsv2rgb(miColor i, miColor *o)
{
  int h;
  float f,p,q,t,sf;
 
  o->a = i.a;

  if(i.r<0) {
  o->r = o->g = o->b = 0;
  }
 
  if(i.g==0) {
    o->r = o->g = o->b = i.b;
    return;
  }
 
  i.r /= 60.0f;
  h = int(floor(i.r));
  f = i.r - h;
  sf = i.g*f;
  p = i.b * (1 - i.g);
  q = i.b * (1 - sf);
  t = i.b * (1 - i.g + sf);
 
  switch(h){
    case 0: o->r = i.b;
            o->g = t;
            o->b = p;
            break;
    case 1: o->r = q;
            o->g = i.b;
            o->b = p;
            break;
    case 2: o->r = p;
            o->g = i.b;
            o->b = t;
            break;
    case 3: o->r = p;
            o->g = q;
            o->b = i.b;
            break;
    case 4: o->r = t;
            o->g = p;
            o->b = i.b;
            break;
    default:o->r = i.b;
            o->g = p;
            o->b = q;
            break;
  }

  //be sure there is no negative value
  o->r = o->r<0?0:o->r;
  o->g = o->g<0?0:o->g;
  o->b = o->b<0?0:o->b;
}

void ccg_mix20layers_colorCorrection(miColor *i, miScalar h, miScalar s, miScalar v, miScalar gamma)
{
  miColor hsv;
 
  if(i->r == 0 && i->g == 0 && i->b == 0) return;
   
  //hsv
  if(h!=0.0f || s!=1.0f || v!=1.0f){
    ccg_mix20layers_rgb2hsv(*i, &hsv);
    if(h!=0.0f) hsv.r = fmod(hsv.r + h + 360.0f, 360.0f);
    if(s!=1.0f) hsv.g = hsv.g * s;
    if(v!=1.0f) hsv.b = hsv.b * v;
    ccg_mix20layers_hsv2rgb(hsv, i);
  }
 
  //gamma
  if(gamma!=1.0f){
    i->r = pow(i->r, 1.0f/gamma);
    i->g = pow(i->g, 1.0f/gamma);
    i->b = pow(i->b, 1.0f/gamma);
  }
}

void ccg_mix20layers_contrastFormula(miScalar *i, miScalar contrast, miScalar power)
{
  float value;
  value = (*i > 0.5f) ? (1.0f - *i) : *i;
  value = 0.5f * pow ( 2.0f * value , power ); 
  *i = (*i > 0.5f) ? (1.0f - value) : value;
}

void ccg_mix20layers_contrastBrightness(miColor *i, miScalar contrast, miScalar brightness)
{
  if(brightness!=1){
    i->r *= brightness;
    i->g *= brightness;
    i->b *= brightness;
  }

  if(contrast!=0){
    //clip color
  i->r = i->r>1?1:(i->r<0?0:i->r);
  i->g = i->g>1?1:(i->g<0?0:i->g);
  i->b = i->b>1?1:(i->b<0?0:i->b);

    miScalar power;
    if (contrast < 0){
      power = 1.0f +  2.0f * contrast;
    }else{
        power = (contrast == 0.5f) ? 0.5f : 1.0f / (1.0f - 2.0f*contrast);
      }
    ccg_mix20layers_contrastFormula(&(i->r), contrast, power);
    ccg_mix20layers_contrastFormula(&(i->g), contrast, power);
    ccg_mix20layers_contrastFormula(&(i->b), contrast, power);
  }
}

void ccg_mix20layers_colorInvert(miColor *i)
{
  i->r = 1 - i->r;
  i->g = 1 - i->g;
  i->b = 1 - i->b;
}


extern "C" DLLEXPORT miBoolean ccg_mix20layers(
  miColor   *result,
  miState   *state,
  struct    ccg_mix20layers *paras)
{
  miColor LayerResult;
  LayerResult.r = LayerResult.g = LayerResult.b = LayerResult.a = 0.0;
  miColor A, B;
 
  miInteger n_layers = *mi_eval_integer(&paras->g_layers_number);

  for (int i = 0; i < n_layers; i++)
  {
    if(*mi_eval_boolean(&paras->Enable[i]))
    {
      A = LayerResult;
      B = *mi_eval_color(&paras->Layer[i]);

    miColor opac;
    if(*mi_eval_boolean(&paras->useOpacity[i])){
      opac = *mi_eval_color(&paras->Opacity[i]);
    opac.a = 1 - (opac.r*0.299f + opac.g*0.587f + opac.b*0.114f);
    ccg_mix20layers_colorInvert(&opac); //invert transparency for maya
    } else opac.r = opac.g = opac.b = opac.a = B.a;
    opac.r = opac.r>1?1:(opac.r<0?0:opac.r);  //clip alpha
    opac.g = opac.g>1?1:(opac.g<0?0:opac.g);
    opac.b = opac.b>1?1:(opac.b<0?0:opac.b);
    opac.a = opac.a>1?1:(opac.a<0?0:opac.a);

    //inverse alpha?
    if(*mi_eval_boolean(&paras->alphaInvert[i])) {
      ccg_mix20layers_colorInvert(&opac);
      opac.a = 1 - opac.a;
    }

    //mix between two layers
    miScalar mix_blend = *mi_eval_scalar(&paras->mix[i]);
    if(mix_blend!=1) {
      opac.r *= mix_blend;
      opac.g *= mix_blend;
      opac.b *= mix_blend;
      opac.a *= mix_blend;
    }

    //color correct?
      if(*mi_eval_boolean(&paras->enable_cc[i])){
        ccg_mix20layers_colorCorrection(&B, *mi_eval_scalar(&paras->hue[i]),
                                            *mi_eval_scalar(&paras->saturation[i]),
                                            *mi_eval_scalar(&paras->value[i]),
                                            *mi_eval_scalar(&paras->gamma[i]) );
      }
     
      //inverse color?
      if(*mi_eval_boolean(&paras->colorInvert[i])) ccg_mix20layers_colorInvert(&B);
     
      miInteger Mode = *mi_eval_integer(&paras->BlendMode[i]);
      miInteger AlphaMode = *mi_eval_integer(&paras->AlphaMode[i]);

      switch(Mode) {
        case 0:   // Normal
                  LayerResult.r = opac.r * B.r + (1 - opac.r) * A.r;
                  LayerResult.g = opac.g * B.g + (1 - opac.g) * A.g;
                  LayerResult.b = opac.b * B.b + (1 - opac.b) * A.b;
                  break;
        case 1:   // Darken              
                  LayerResult.r = opac.r * ((A.r < B.r) ? A.r : B.r) + (1 - opac.r) * A.r;
                  LayerResult.g = opac.g * ((A.g < B.g) ? A.g : B.g) + (1 - opac.g) * A.g;
                  LayerResult.b = opac.b * ((A.b < B.b) ? A.b : B.b) + (1 - opac.b) * A.b;
                  break;
        case 2:   // Multiply
              LayerResult.r = opac.r * (A.r * B.r) + (1 - opac.r) * A.r;
              LayerResult.g = opac.g * (A.g * B.g) + (1 - opac.g) * A.g;
              LayerResult.b = opac.b * (A.b * B.b) + (1 - opac.b) * A.b;         
              break;
        case 3:   // Color burn
              LayerResult.r = opac.r * (1 - (1 - A.r) / B.r) + (1 - opac.r) * A.r;
              LayerResult.g = opac.g * (1 - (1 - A.g) / B.g) + (1 - opac.g) * A.g;
              LayerResult.b = opac.b * (1 - (1 - A.b) / B.b) + (1 - opac.b) * A.b;
              break;
        case 4:   // Inverse Color burn
              LayerResult.r = opac.r * (1 - (1 - B.r) / A.r) + (1 - opac.r) * A.r;
              LayerResult.g = opac.g * (1 - (1 - B.g) / A.g) + (1 - opac.g) * A.g;
              LayerResult.b = opac.b * (1 - (1 - B.b) / A.b) + (1 - opac.b) * A.b;
              break;
        case 5:   // Subtract
              LayerResult.r = opac.r * (A.r + B.r - 1.0f) + (1 - opac.r) * A.r;
              LayerResult.g = opac.g * (A.g + B.g - 1.0f) + (1 - opac.g) * A.g;
              LayerResult.b = opac.b * (A.b + B.b - 1.0f) + (1 - opac.b) * A.b;      
              break;
        case 6:   // Add
              LayerResult.r = opac.r * (A.r + B.r) + (1 - opac.r) * A.r;
              LayerResult.g = opac.g * (A.g + B.g) + (1 - opac.g) * A.g;
              LayerResult.b = opac.b * (A.b + B.b) + (1 - opac.b) * A.b;       
              break;
        case 7:   // Lighten       
              LayerResult.r = opac.r * ((A.r > B.r) ? A.r : B.r) + (1 - opac.r) * A.r;
              LayerResult.g = opac.g * ((A.g > B.g) ? A.g : B.g) + (1 - opac.g) * A.g;
              LayerResult.b = opac.b * ((A.b > B.b) ? A.b : B.b) + (1 - opac.b) * A.b;     
              break;
        case 8:   // Screen
              LayerResult.r = opac.r * (1 - (1 - A.r) * (1-B.r)) + (1 - opac.r) * A.r;
              LayerResult.g = opac.g * (1 - (1 - A.g) * (1-B.g)) + (1 - opac.g) * A.g;
              LayerResult.b = opac.b * (1 - (1 - A.b) * (1-B.b)) + (1 - opac.b) * A.b;
              break;
        case 9:   // Color Dodge
              LayerResult.r = opac.r * (A.r / (1 - B.r)) + (1 - opac.r) * A.r;
              LayerResult.g = opac.g * (A.g / (1 - B.g)) + (1 - opac.g) * A.g;
              LayerResult.b = opac.b * (A.b / (1 - B.b)) + (1 - opac.b) * A.b;
              break;
        case 10:  // Inverse Color Dodge
              LayerResult.r = opac.r * (B.r / (1 - A.r)) + (1 - opac.r) * A.r;
              LayerResult.g = opac.g * (B.g / (1 - A.g)) + (1 - opac.g) * A.g;
              LayerResult.b = opac.b * (B.b / (1 - A.b)) + (1 - opac.b) * A.b;
              break;
        case 11:  // Overlay
              LayerResult.r = opac.r * ((A.r<0.5f)? LayerResult.r =
                A.r * 2.0f * B.r : 1.0f - 2.0f*(1.0f-A.r)*(1.0f-B.r)) + (1 - opac.r) * A.r;
              LayerResult.g = opac.g * ((A.g<0.5f)? LayerResult.g =
                A.g * 2.0f * B.g : 1.0f - 2.0f*(1.0f-A.g)*(1.0f-B.g)) + (1 - opac.g) * A.g;
              LayerResult.b = opac.b * ((A.b<0.5f)? LayerResult.b =
                A.b * 2.0f * B.b : 1.0f - 2.0f*(1.0f-A.b)*(1.0f-B.b)) + (1 - opac.b) * A.b;    
              break;
        case 12:  // Soft Light
              LayerResult.r = opac.r * ((B.r<0.5f)? LayerResult.r =
                2.0f*A.r*B.r+powf(A.r,2.0f)*(1.0f-2.0f*B.r):
                sqrtf(A.r)*(2.0f*B.r-1)+(2.0f*A.r)*(1.0f-B.r)) + (1 - opac.r) * A.r;
              LayerResult.g = opac.g * ((B.g<0.5f)? LayerResult.g =
                2.0f*A.g*B.g+powf(A.g,2.0f)*(1.0f-2.0f*B.g):
                sqrtf(A.g)*(2.0f*B.g-1)+(2.0f*A.g)*(1.0f-B.g)) + (1 - opac.g) * A.g;
              LayerResult.b = opac.b * ((B.b<0.5f)? LayerResult.b =
                2.0f*A.b*B.b+powf(A.b,2.0f)*(1.0f-2.0f*B.b):
                sqrtf(A.b)*(2.0f*B.b-1)+(2.0f*A.b)*(1.0f-B.b)) + (1 - opac.b) * A.b;
              break;
        case 13:  // Hard Light
              LayerResult.r = opac.r * ((A.r<0.5f)? LayerResult.r = A.r*2.0f*B.r:
                  1.0f - 2.0f*(1.0f-A.r)*(1.0f-B.r)) + (1 - opac.r) * A.r;
              LayerResult.g = opac.g * ((A.g<0.5f)? LayerResult.g = A.g*2.0f*B.g:
                  1.0f - 2.0f*(1.0f-A.g)*(1.0f-B.g)) + (1 - opac.g) * A.g;
              LayerResult.b = opac.b * ((A.b<0.5f)? LayerResult.b = A.b*2.0f*B.b:
                  1.0f - 2.0f*(1.0f-A.b)*(1.0f-B.b)) + (1 - opac.b) * A.b;
              break;
        case 14:  // Reflect
              LayerResult.r = opac.r * ((B.r==1.0f)? LayerResult.r = 1.0f: LayerResult.r =
                powf(A.r,2)/(1.0f-B.r)) + (1 - opac.r) * A.r;

              LayerResult.g = opac.g * ((B.g==1.0f)? LayerResult.g = 1.0f: LayerResult.g =
                powf(A.g,2)/(1.0f-B.g)) + (1 - opac.g) * A.g;

              LayerResult.b = opac.b * ((B.b==1.0f)? LayerResult.b = 1.0f: LayerResult.b =
                powf(A.b,2)/(1.0f-B.b)) + (1 - opac.b) * A.b;
              break;
        case 15:  // Glow
              LayerResult.r = opac.r * ((A.r==1.0f)? LayerResult.r = 1.0f: LayerResult.r =
                powf(B.r,2)/(1.0f-A.r)) + (1 - opac.r) * A.r;

              LayerResult.g = opac.g * ((A.g==1.0f)? LayerResult.g = 1.0f: LayerResult.g =
                powf(B.g,2)/(1.0f-A.g)) + (1 - opac.g) * A.g;

              LayerResult.b = opac.b * ((A.b==1.0f)? LayerResult.b = 1.0f: LayerResult.b =
                powf(B.b,2)/(1.0f-A.b)) + (1 - opac.b) * A.b;
              break;
        case 16:  // Average
              LayerResult.r = opac.r * ((A.r + B.r) / 2.0f) + (1 - opac.r) * A.r;
              LayerResult.g = opac.g * ((A.g + B.g) / 2.0f) + (1 - opac.g) * A.g;
              LayerResult.b = opac.b * ((A.b + B.b) / 2.0f) + (1 - opac.b) * A.b;          
              break;
        case 17:  // Difference
              LayerResult.r = opac.r * (fabsf(A.r - B.r)) + (1 - opac.r) * A.r;
              LayerResult.g = opac.g * (fabsf(A.g - B.g)) + (1 - opac.g) * A.g;
              LayerResult.b = opac.b * (fabsf(A.b - B.b)) + (1 - opac.b) * A.b;
              break;
        case 18:  // Exclusion
              LayerResult.r = opac.r * (A.r + B.r - (2 * A.r * B.r)) + (1 - opac.r) * A.r;
              LayerResult.g = opac.g * (A.g + B.g - (2 * A.g * B.g)) + (1 - opac.g) * A.g;
              LayerResult.b = opac.b * (A.b + B.b - (2 * A.b * B.b)) + (1 - opac.b) * A.b;     
              break;
        default:  LayerResult = A;
              return miTRUE;
      }//blend mode

      switch(AlphaMode) {
          case 0: // normal
              LayerResult.a = opac.a + (1-opac.a)*LayerResult.a;
              break;
          case 1: // Dont add to Alpha       
              LayerResult.a = A.a;       
              break;
          case 2: // Use highest Alpha       
              LayerResult.a = opac.a * ((A.a > B.a) ? A.a : B.a) + (1 - opac.a) * A.a;       
              break;
          case 3: // Add difference to highest Alpha       
              LayerResult.a = opac.a*((A.a > B.a) ? (A.a+(A.a-B.a)) : (B.a+(B.a-A.a))) + (1-opac.a)*A.a;
              break;
          case 4: // Full Alpha      
              LayerResult.a = 1.0;       
              break;
          default:  LayerResult.a = A.a;
                    return miTRUE;
      }//alpha mode
     
    }//layer enabled
  }// for loop
 
  if(*mi_eval_boolean(&paras->g_inverse_alpha)) LayerResult.a = 1 - LayerResult.a;
  if(*mi_eval_boolean(&paras->g_enable_cc)){
    ccg_mix20layers_colorCorrection(&LayerResult, *mi_eval_scalar(&paras->g_hue),
                          *mi_eval_scalar(&paras->g_saturation),
                          *mi_eval_scalar(&paras->g_value),
                          *mi_eval_scalar(&paras->g_gamma) );
    ccg_mix20layers_contrastBrightness(&LayerResult, *mi_eval_scalar(&paras->g_contrast),
                            *mi_eval_scalar(&paras->g_brightness));
  }

  miBoolean ColorClip = *mi_eval_boolean(&paras->ColorClip);
  if (ColorClip == 1)
  {
    if (LayerResult.r > 1.0) {LayerResult.r = 1.0;}
    else if (LayerResult.r < 0.0) {LayerResult.r = 0.0;}
    if (LayerResult.g > 1.0) {LayerResult.g = 1.0;}
    else if (LayerResult.g < 0.0) {LayerResult.g = 0.0;}
    if (LayerResult.b > 1.0) {LayerResult.b = 1.0;}
    else if (LayerResult.b < 0.0) {LayerResult.b = 0.0;}
    if (LayerResult.a > 1.0) {LayerResult.a = 1.0;}
    else if (LayerResult.a < 0.0) {LayerResult.a = 0.0;}
  }

  *result = LayerResult;
  return miTRUE;
}
