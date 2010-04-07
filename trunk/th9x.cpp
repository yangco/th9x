/*
 * Author	Thomas Husterer <thus1@t-online.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

bugs:
- overflow in mixer
+ no-trim in pos-neg beruecksichtigen?
+ submenu in calib
todo
- doku einschaltverhalten, trainermode
- stat mit times
- trainer
- pcm 
- light auto off
- move/copy model
done
+ silverlit
+ bat spanng. calib
- timer stop/start mit switch
+ timer beep
+ pos-neg-abs, in mixer anzeigen
+ light schalten
+ vers num, date
+ exit fuer beenden von subsub..
+ INV as revert, - as don't cares
+ optimierte eeprom-writes
+ edit model name
+ menu lang reduzieren, seitl. move mit <->
+ limits
+ model  mode?  THR RUD ELE AIL
+ bug: ch1 verschwindet in mixall, csr laeuft hinter ch8 
+ expo dr algo
+ icons?
+ mix mit weight
+ delete line in mix
+ alphanum skip signs
+ plus-minus mixers mit flag
+ scr, dst je 4 bit 
+ src const 100
+ menus mit nummerierung 1/4 > m
+ trim mit >, <clear
+ mix mit kombizeile
+ negativ switches, constant switch, 
+ trim def algo
+ mixer algo
+ beep
+ vBat limit + warning
+ eeprom
+ trim val unit , hidden quadrat
+ mix list CH1 += LH +100  assym?
+ drSwitches als funktion
+ philosophie: Menuselect=Menu Lang,  Chain=Menu kurz, Pop=Exit kurz, Back=Exit Lang
+ model names
+ expo/dr exp1,exp2,dr-sw  3bytes
+ blink
+ switches as key
+ killEvents
+ calib menu
+ contrast
 */

#include "th9x.h"

/*
mode1 rud ele thr ail
mode2 rud thr ele ail
mode3 ail ele thr rud
mode4 ail thr ele rud
*/



EEGeneral g_eeGeneral;
ModelData g_model;




const prog_char modi12x3[]="RUDELETHRAILRUDTHRELEAILAILELETHRRUDAILTHRELERUD";


void putsTime(uint8_t x,uint8_t y,int16_t tme,uint8_t att,uint8_t att2)
{
  lcd_putc(      x+ 0*FW, y, tme<0 ?'-':' ');
  lcd_outdezNAtt(x+ 3*FW, y, abs(tme)/60,LEADING0+att,2);
  lcd_outdezNAtt(x+ 6*FW, y, abs(tme)%60,LEADING0+att2,2);
  lcd_putc(      x+ 3*FW, y, ':');
}
void putsVBat(uint8_t x,uint8_t y,uint8_t att)
{
  lcd_putc(      x+ 4*FW,   y,    'V');
  lcd_outdezAtt( x+ 4*FW,   y,    g_vbat100mV,att|PREC1);
  //lcd_plot(    x+ 3*FWNUM,   y+7);//komma
  // lcd_plot(    x+ 3*FW-1, y+6);
  // lcd_plot(    x+ 3*FW-1, y+7);
  
}
void putsChnRaw(uint8_t x,uint8_t y,uint8_t idx1,uint8_t att)
{
  if((idx1>=1) && (idx1 <=4)) 
  {
    lcd_putsnAtt(x,y,modi12x3+g_model.stickMode*12+3*(idx1-1),3,att);  
  }else{
    lcd_putsnAtt(x,y,PSTR(" P1 P2 P3MAX")+3*(idx1-5),3,att);  
  }
}
void putsChn(uint8_t x,uint8_t y,uint8_t idx1,uint8_t att)
{
  lcd_putsnAtt(x,y,PSTR("   CH1CH2CH3CH4CH5CH6CH7CH8")+3*idx1,3,att);  
}
void putsDrSwitches(uint8_t x,uint8_t y,int8_t idx1,uint8_t att)
{
  lcd_putcAtt(x,y, idx1<0 ? '!' : ' ',att);  
  lcd_putsnAtt(x+FW,y,PSTR(SWITCHES_STR)+4*abs(idx1),4,att);  
}
bool getSwitch(int8_t swtch, bool nc)
{
  if(swtch==0)return nc; 
  if(swtch<0) return ! keyState((EnumKeys)(SW_BASE-swtch));
  return keyState((EnumKeys)(SW_BASE+swtch));
}
void checkSwitches()
{
  uint8_t i;
  for(i=SW_BASE_DIAG; i< SW_Trainer; i++)
  {
    if(i==SW_ID0) continue;
    if(getSwitch(i-SW_BASE,0)) break;
  }
  if(i==SW_Trainer) return;
  //alert(PSTR("switch error"));
  beepErr();
  pushMenu(menuProcDiagKeys);
}



MenuFuncP g_menuStack[5]
#ifdef SIM
 = {menuProc0};
#endif
;
uint8_t  g_menuStackPtr = 0;
// uint8_t  g_menuStackSub[5];
uint8_t  g_beepCnt;

void alert(const char_P * s)
{
  lcd_clear();
  lcd_puts_P(50,0*FH,PSTR("ALERT"));  
  lcd_puts_P(0,3*FW,s);  
  lcd_puts_P(0,7*FH,PSTR("PRESS ANY KEY"));  
  refreshDiplay();
  beepErr();
  while(1)
  {
    if(~PINB & 0x7e) //(1<<INP_B_KEY_MEN | 1<<INP_B_KEY_EXT))
      return;
#ifdef SIM
void doFxEvents();
//printf("pinb %x\n",PINB);
    doFxEvents();
#endif    
  }
}
uint8_t checkTrim(uint8_t event)
{
  int8_t k = (event & EVT_KEY_MASK) - TRM_BASE;
  if((k>=0) && (k<8) && (event & _MSK_KEY_REPT))
  {
    //LH_DWN LH_UP LV_DWN LV_UP RV_DWN RV_UP RH_DWN RH_UP
    uint8_t idx = k/2;
    bool    up  = k&1;
    //if(idx==3) dwn=!dwn;
    if(!up){
      if(g_model.trimData[idx].trim > -31){
        g_model.trimData[idx].trim--;
        STORE_MODELVARS;
        beep();
      }
    }else{
      if(g_model.trimData[idx].trim < 31){
        g_model.trimData[idx].trim++;
        STORE_MODELVARS;
        beep();
      }
    }
    if(g_model.trimData[idx].trim==0) {
      killEvents(event);
      beepWarn();
    }
    return 0;
  }
  return event;
}


bool checkIncDecGen2(uint8_t event, void *i_pval, int16_t i_min, int16_t i_max, uint8_t i_flags)
{
  int16_t val = i_flags & _FL_SIZE2 ? *(int16_t*)i_pval : *(int8_t*)i_pval ;
  int16_t newval = val;
  if(i_flags&_FL_VERT)
    switch(event)
    {
      case  EVT_KEY_FIRST(KEY_UP):
      case  EVT_KEY_REPT(KEY_UP):    newval++; beep();     break;
      case  EVT_KEY_FIRST(KEY_DOWN):
      case  EVT_KEY_REPT(KEY_DOWN):  newval--; beep();     break;
    }
  else  
    switch(event)
    {
      case  EVT_KEY_FIRST(KEY_RIGHT):
      case  EVT_KEY_REPT(KEY_RIGHT): newval++; beep();     break;
      case  EVT_KEY_FIRST(KEY_LEFT):
      case  EVT_KEY_REPT(KEY_LEFT):  newval--; beep();     break;
    }

  if(newval>i_max)
  {
     newval = i_max;
     killEvents(event);
     beepWarn();
  }
  if(newval < i_min)
  {
     newval = i_min;
     killEvents(event);
     beepWarn();
  }
  if(newval != val){
    if(newval==0) {
      killEvents(event);
      beepWarn();
    }
    if(i_flags & _FL_SIZE2 ) *(int16_t*)i_pval = newval;
    else                     *( int8_t*)i_pval = newval;
    eeDirty(i_flags & (EE_GENERAL|EE_MODEL));
    return true;
  }
  return false;
}

int16_t checkIncDec_hm(uint8_t event, int16_t i_val, int16_t i_min, int16_t i_max)
{
  checkIncDecGen2(event,&i_val,i_min,i_max,_FL_SIZE2|EE_MODEL);
  return i_val;
}
int16_t checkIncDec_vm(uint8_t event, int16_t i_val, int16_t i_min, int16_t i_max)
{
  checkIncDecGen2(event,&i_val,i_min,i_max,_FL_SIZE2|_FL_VERT|EE_MODEL);
  return i_val;
}

uint8_t checkSubGen(uint8_t event,uint8_t num, uint8_t sub, bool vert)
{
  uint8_t subOld=sub;
  uint8_t inc = vert ?  KEY_DOWN : KEY_RIGHT;
  uint8_t dec = vert ?  KEY_UP   : KEY_LEFT;
  
  if(event==EVT_KEY_REPT(inc) || event==EVT_KEY_FIRST(inc))
  {
    beep();
    if(sub < (num-1)) {
      (sub)++;
    }else{
      if(event==EVT_KEY_REPT(inc))
      {
        beepWarn();
        killEvents(event);
      }else{
        (sub)=0;
      }
    }
  }
  else if(event==EVT_KEY_REPT(dec) || event==EVT_KEY_FIRST(dec))
  {
    beep();
    if(sub > 0) {
      (sub)--;
    }else{
      if(event==EVT_KEY_REPT(dec))
      {
        beepWarn();
        killEvents(event);
      }else{
        (sub)=(num-1);
      }
    }
  }
  else if(event==EVT_ENTRY)
  {
    sub = 0;
  }
  if(subOld!=sub) BLINK_SYNC;
  return sub;//false;
}

void popMenu()
{
  if(g_menuStackPtr>0){
    g_menuStackPtr--;
    beep();  
    (*g_menuStack[g_menuStackPtr])(EVT_ENTRY_UP);
  }else{
    alert(PSTR("menuStack underflow"));
  }
}
void chainMenu(MenuFuncP newMenu)
{
  g_menuStack[g_menuStackPtr] = newMenu;
  (*newMenu)(EVT_ENTRY);
  beep();
}
void pushMenu(MenuFuncP newMenu)
{
  g_menuStackPtr++;
  if(g_menuStackPtr >= DIM(g_menuStack))
  {
    g_menuStackPtr--;
    alert(PSTR("menuStack overflow"));
    return;
  }
  beep();
  g_menuStack[g_menuStackPtr] = newMenu;
  (*newMenu)(EVT_ENTRY);
}





void perMain()
{
  perOut();
  eeCheck();
  //if(! g_menuStack[0]) g_menuStack[0] =  menuProc0;

  lcd_clear();
  uint8_t evt=getEvent();
  evt = checkTrim(evt);
  g_menuStack[g_menuStackPtr](evt);
  refreshDiplay();
}

#ifndef SIM
#include <avr/interrupt.h>

extern uint16_t g_tmr1Latency;
//ISR(TIMER1_OVF_vect)
ISR(TIMER1_COMPA_vect) //2MHz
{
  static uint8_t   pulsePol;
  static uint16_t *pulsePtr = pulses2MHz;
  uint16_t dt=TCNT1;//-OCR1A;
  g_tmr1Latency = max(dt,g_tmr1Latency);

  if(pulsePol)
  {
    PORTB |=  (1<<OUT_B_PPM);
    pulsePol = 0;
  }else{
    PORTB &= ~(1<<OUT_B_PPM);
    pulsePol = 1;
  }

  OCR1A  = *pulsePtr++;

  if( *pulsePtr == 0) {
    //currpulse=0;
    pulsePtr = pulses2MHz;
    pulsePol = 0;

    TIMSK &= ~(1<<OCIE1A);
    sei();
    setupPulses();
    cli();
    TIMSK |= (1<<OCIE1A);
  }
}

#if 0
static uint8_t  currpulse;
extern uint16_t g_tmr1Latency;
//ISR(TIMER1_OVF_vect)
ISR(TIMER1_COMPA_vect) //2MHz
{
  uint16_t dt=TCNT1;//-OCR1A;
  g_tmr1Latency = max(dt,g_tmr1Latency);

  if(currpulse & 1)
  {
    PORTB |=  (1<<OUT_B_PPM);
  }else{
    PORTB &= ~(1<<OUT_B_PPM);
    //dt = 300*2;
  }
  dt = pulses2MHz[currpulse++];
  OCR1A  = dt;

  uint16_t dt2=pulses2MHz[currpulse];
  if( dt2 == 0) {
    currpulse=0;
    TIMSK &= ~(1<<OCIE1A);
    sei();
    setupPulses();
    cli();
    TIMSK |= (1<<OCIE1A);
  }
}
#endif
volatile uint8_t g_tmr16KHz;

ISR(TIMER0_OVF_vect) 
{
  g_tmr16KHz++;
}
uint16_t getTmr16KHz()
{
  while(1){
    uint8_t hb  = g_tmr16KHz;
    uint8_t lb  = TCNT0;
    if(hb-g_tmr16KHz==0) return (hb<<8)|lb;
  }
}
ISR(TIMER0_COMP_vect) //10ms
{
  TIMSK &= ~(1<<OCIE0);
  OCR0 = OCR0 + 156;
  sei();
  if(g_beepCnt){
    g_beepCnt--;
    PORTE |=  (1<<OUT_E_BUZZER);
  }else{
    PORTE &= ~(1<<OUT_E_BUZZER);
  }
  per10ms();
  cli();
  TIMSK |= (1<<OCIE0);
}
  

extern uint16_t g_timeMain;
int main()
{
  DDRA = 0xff;  PORTA = 0x00;
  DDRB = 0x81;  PORTB = 0x7e; //pullups keys+nc
  DDRC = 0x3e;  PORTC = 0xc1; //pullups nc
  DDRD = 0x00;  PORTD = 0xff; //pullups keys
  DDRE = 0x08;  PORTE = 0xff-(1<<OUT_E_BUZZER); //pullups + buzzer 0
  DDRF = 0x00;  PORTF = 0xff; //anain
  DDRG = 0x10;  PORTG = 0xff-(1<<OUT_G_SIM_CTL); //pullups + SIM_CTL 0
  lcd_init();

  // TCNT0         10ms = 16MHz/160000
  //TCCR0  = (1<<WGM01)|(7 << CS00);//  CTC mode, clk/1024
  TCCR0  = (7 << CS00);//  Norm mode, clk/1024
  OCR0   = 156;
  TIMSK |= (1<<OCIE0) |  (1<<TOIE0);

  // TCNT1 2MHz
  TCCR1A = (0<<WGM10);
  TCCR1B = (1 << WGM12) | (2<<CS10); // CTC OCR1A, 16MHz / 8
  TIMSK |= (1<<OCIE1A);
  //OCR1AH = (300*2-1)>>8;
  //OCR1AL = (300*2-1);

  sei(); //damit alert in eeReadGeneral() nicht haengt
  g_menuStack[0] =  menuProc0;
  eeReadAll();

  
  uint16_t old10ms;
  checkSwitches();
  setupPulses();
  while(1){
    old10ms=g_tmr10ms;
    uint16_t t0 = getTmr16KHz();
    perMain();
    t0 = getTmr16KHz() - t0;
    g_timeMain = max(g_timeMain,t0);
    while(g_tmr10ms==old10ms) sleep_mode();
    //while(g_tmr10ms==old10ms) sleep_mode();
  }
}
#endif