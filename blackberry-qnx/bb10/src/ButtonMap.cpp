#include <screen/screen.h>
#include <bps/screen.h>
#include <bps/navigator.h>
#include <bps/bps.h>

#include "ButtonMap.h"
#include "RetroArch-Cascades.h"
#include "input/input_common.h"
#include "../../frontend_qnx.h"

ButtonMap::ButtonMap(screen_context_t screen_ctx, QString groupId, int coid)
{
   this->screen_cxt = screen_ctx;
   this->groupId = groupId;
   this->coid = coid;

   const int usage = SCREEN_USAGE_NATIVE | SCREEN_USAGE_WRITE | SCREEN_USAGE_READ;
   int rc;

   if(screen_create_window_type(&screen_win, screen_cxt, SCREEN_CHILD_WINDOW))
   {
      RARCH_ERR("ButtonMap: screen_create_window_type failed.\n");
   }

   screen_join_window_group(screen_win, (const char *)groupId.toAscii().constData());
   int format = SCREEN_FORMAT_RGBA8888;
   screen_set_window_property_iv(screen_win, SCREEN_PROPERTY_FORMAT, &format);

   screen_set_window_property_iv(screen_win, SCREEN_PROPERTY_USAGE, &usage);

   screen_display_t screen_disp;
   if (screen_get_window_property_pv(screen_win, SCREEN_PROPERTY_DISPLAY, (void **)&screen_disp))
   {
      RARCH_ERR("screen_get_window_property_pv [SCREEN_PROPERTY_DISPLAY] failed.\n");
   }

   if (screen_get_display_property_iv(screen_disp, SCREEN_PROPERTY_SIZE, screen_resolution))
   {
      RARCH_ERR("screen_get_window_property_iv [SCREEN_PROPERTY_SIZE] failed.\n");
   }

   rc = screen_set_window_property_iv(screen_win, SCREEN_PROPERTY_BUFFER_SIZE, screen_resolution);
   if (rc) {
      perror("screen_set_window_property_iv");
   }

   int z = -10;
   if (screen_set_window_property_iv(screen_win, SCREEN_PROPERTY_ZORDER, &z) != 0) {
      return;
   }

   rc = screen_create_window_buffers(screen_win, 1);
   if (rc) {
      perror("screen_create_window_buffers");
   }

   screen_get_window_property_pv(screen_win, SCREEN_PROPERTY_RENDER_BUFFERS, (void **)&screen_buf);

   int bg[] = { SCREEN_BLIT_COLOR, 0x00000000,
                SCREEN_BLIT_GLOBAL_ALPHA, 0x80,
                SCREEN_BLIT_END };
   screen_fill(screen_cxt, screen_buf, bg);

   screen_post_window(screen_win, screen_buf, 1, screen_resolution, 0);

   buttonDataModel = new ArrayDataModel();

   refreshButtonMap();
}

ButtonMap::~ButtonMap()
{

}

QString ButtonMap::getLabel(int button)
{
   return QString((uint)platform_keys[button].joykey);
}

//pass in RARCH button enum for button, map to g_setting
 int ButtonMap::mapNextButtonPressed()
 {
    bps_event_t *event = NULL;
    int sym;

    //use in frontend run loop, get key pressed back, and map
    int z = 10;
    if (screen_set_window_property_iv(screen_win, SCREEN_PROPERTY_ZORDER, &z) != 0)
    {
       return -1;
    }

    screen_post_window(screen_win, screen_buf, 1, screen_resolution, 0);

    while(1){
       if (BPS_SUCCESS != bps_get_event(&event, -1))
       {
          fprintf(stderr, "bps_get_event failed\n");
          break;
       }

       if (event)
       {
          int domain = bps_event_get_domain(event);

          if (domain == screen_get_domain())
          {
             screen_event_t screen_event = screen_event_get_event(event);
             int screen_val;
             screen_get_event_property_iv(screen_event, SCREEN_PROPERTY_TYPE, &screen_val);

             //TODO: Should we only let the buttons through that we are trying to map?
             if(screen_val == SCREEN_EVENT_MTOUCH_TOUCH)
             {
                //This is touch screen event
                sym = -1;
                break;
             }
             else if(screen_val == SCREEN_EVENT_KEYBOARD)
             {
                screen_get_event_property_iv(screen_event, SCREEN_PROPERTY_KEY_SYM, &sym);
                sym &= 0xFF;
                break;
             }
             else if( (screen_val == SCREEN_EVENT_GAMEPAD) || (screen_val == SCREEN_EVENT_JOYSTICK) )
             {
                screen_get_event_property_iv(screen_event, SCREEN_PROPERTY_BUTTONS, &sym);
                break;
             }
          }
       }
    }

    z = -10;
    if (screen_set_window_property_iv(screen_win, SCREEN_PROPERTY_ZORDER, &z) != 0) {
       return -1;
    }

    screen_post_window(screen_win, screen_buf, 1, screen_resolution, 0);

    return (g_settings.input.binds[player][button].joykey = sym);
 }

 int ButtonMap::getButtonMapping(int player, int button)
 {
    return g_settings.input.binds[player][button].joykey;
 }

 QString ButtonMap::buttonToString(int button)
 {
    for(int i=0;i<20;++i)
    {
       if(platform_keys[i].joykey == (uint)button)
       {
          return QString(platform_keys[i].desc);
       }
    }

    return (button!=NO_BTN) ? QString(button) : QString("Not Mapped");
 }

 int ButtonMap::requestButtonMapping(screen_device_t device, int player, int button)
 {
    //Send message to run thread to start mapping, wait for reply.
    recv_msg msg;
    msg.code = RETROARCH_BUTTON_MAP;

    this->device = device;
    this->player = player;
    this->button = button;

    return MsgSend(coid, (void*)&msg, sizeof(msg), (void*)NULL, 0);
 }

void ButtonMap::mapDevice(int index, int player)
{
   if (input_qnx.set_keybinds)
      input_qnx.set_keybinds((void*)&devices[index], devices[index].device, player, 0,
         (1ULL << KEYBINDS_ACTION_SET_DEFAULT_BINDS));

   refreshButtonMap();
}

 void ButtonMap::refreshButtonMap()
 {
    QVariantMap map;

    buttonDataModel->clear();

    for(int i=0; i<16; ++i)
    {
       QString desc = QString(input_config_bind_map[i].desc);
       int index = desc.indexOf("(");
       if(index != -1)
       {
          desc.truncate(index);
       }

       map.insert("label",QVariant(desc));
       map.insert("button", buttonToString(g_settings.input.binds[0][i].joykey));
       map.insert("type", QVariant("item"));
       map.insert("index", QVariant(i));
       buttonDataModel->append(map);
    }

    map.insert("label",QVariant("RetroArch Menu"));
    map.insert("button", buttonToString(g_settings.input.binds[0][RARCH_MENU_TOGGLE].joykey));
    map.insert("type", QVariant("item"));
    map.insert("index", QVariant(RARCH_MENU_TOGGLE));
    buttonDataModel->append(map);
 }
