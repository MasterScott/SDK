#include "ModioUtility.h"
#include "c/creators/ModioFilterCreator.h"

namespace modio
{
void onUpdateCurrentUser(void *object, ModioResponse response, ModioUser user)
{
  if (response.code >= 200 && response.code < 300)
  {
    modio::writeLogLine("Current user updated sucessfully.", MODIO_DEBUGLEVEL_LOG);
  }
  else
  {
    modio::writeLogLine("Could not update current user.", MODIO_DEBUGLEVEL_WARNING);
  }
}

void onUpdateCurrentUserRatings(void *object, ModioResponse response, ModioRating *ratings_array, u32 ratings_array_size)
{
  if(response.result_offset == 0) /* Clear only if is first result page */
    modio::current_user_ratings.clear();

  if (response.code >= 200 && response.code < 300)
  {
    modio::writeLogLine("Current user ratings retrieved sucessfully.", MODIO_DEBUGLEVEL_LOG);
    for(u32 i=0; i<ratings_array_size; i++)
    {
      if(ratings_array[i].rating == 1)
        modio::current_user_ratings[ratings_array[i].mod_id] = MODIO_RATING_POSITIVE;
      else if(ratings_array[i].rating == -1)
        modio::current_user_ratings[ratings_array[i].mod_id] = MODIO_RATING_NEGATIVE;
      else
        modio::current_user_ratings[ratings_array[i].mod_id] = MODIO_RATING_UNDEFINED;
    }

    if(response.result_offset + response.result_count < response.result_total)
    {
      modio::writeLogLine("Paginated response, retrieving next page...", MODIO_DEBUGLEVEL_LOG);
      ModioFilterCreator filter;
      modioInitFilter(&filter);
      modioSetFilterOffset(&filter, response.result_offset + response.result_count);
      modioGetUserRatings(NULL, filter, &modio::onUpdateCurrentUserRatings);
      modioFreeFilter(&filter);
    }
  }
  else
  {
    modio::writeLogLine("Could not update current user ratings.", MODIO_DEBUGLEVEL_WARNING);
  }
}

void onUpdateCurrentUserSubscriptions(void* object, ModioResponse response, ModioMod *mods, u32 mods_size)
{
  if(response.result_offset == 0) /* Clear only if is first result page */
    modio::current_user_subscriptions.clear();
  
  if (response.code >= 200 && response.code < 300)
  {
    modio::writeLogLine("Current user subscriptions retrieved sucessfully.", MODIO_DEBUGLEVEL_LOG);

    std::string subscription_logs = "";
    for(u32 i=0; i<mods_size; i++)
    {
      modio::current_user_subscriptions.insert(mods[i].id);
      subscription_logs += modio::toString(mods[i].id) + ",";
    }

    modio::writeLogLine("You are subscribed to the following mods: " + subscription_logs, MODIO_DEBUGLEVEL_LOG);

    if(response.result_offset + response.result_count < response.result_total)
    {
      modio::writeLogLine("Paginated response, retrieving next page...", MODIO_DEBUGLEVEL_LOG);
      ModioFilterCreator filter;
      modioInitFilter(&filter);
      modioSetFilterOffset(&filter, response.result_offset + response.result_count);
      modioGetUserSubscriptions(NULL, filter, &modio::onUpdateCurrentUserSubscriptions);
      modioFreeFilter(&filter);
    }
  }
  else
  {
    modio::writeLogLine("Could not update current user ratings.", MODIO_DEBUGLEVEL_WARNING);
  }
}

static void onAddModsToDownloadQueue(void *object, ModioResponse response, ModioMod *mods, u32 mods_size)
{
  std::vector<int>* ptr_mod_ids = (std::vector<int>*)object;
  if (response.code == 200)
  {
    modio::writeLogLine("Mod queue data retrived successfully", MODIO_DEBUGLEVEL_LOG);

    for (u32 i = 0; i < mods_size; i++)
    {
      modio::curlwrapper::queueModDownload(mods[i]);
    }

    if(response.result_offset + response.result_count < response.result_total)
    {
      modio::writeLogLine("Paginated response, retrieving next page...", MODIO_DEBUGLEVEL_LOG);
      ModioFilterCreator filter;
      modioInitFilter(&filter);
      for (auto &mod_id : *ptr_mod_ids)
      {
        modioAddFilterInField(&filter, "id", modio::toString(mod_id).c_str());
      }
      modioSetFilterOffset(&filter, response.result_offset + response.result_count);
      modioGetAllMods(ptr_mod_ids, filter, &modio::onAddModsToDownloadQueue);
      modioFreeFilter(&filter);
    }else
    {
      delete ptr_mod_ids;
    }
  }else
  {
    modio::writeLogLine("Error retrieving mod queue data", MODIO_DEBUGLEVEL_ERROR);
    delete ptr_mod_ids;
  }
}

static void onModsUpdateEvent(void *object, ModioResponse response, ModioMod *mods, u32 mods_size)
{
  std::vector<int>* ptr_mod_ids = (std::vector<int>*)object;
  if (response.code == 200)
  {
    modio::writeLogLine("Updated mods data retrived successfully", MODIO_DEBUGLEVEL_LOG);
    for (u32 i = 0; i < mods_size; i++)
    {
      modio::Mod mod;
      mod.initialize(mods[i]);
      std::string mod_path_str = modio::getInstalledModPath(mod.id) + "modio.json";
      modio::writeJson(mod_path_str, modio::toJson(mod));
      modio::writeLogLine("Mod updated", MODIO_DEBUGLEVEL_LOG);
    }

    if(response.result_offset + response.result_count < response.result_total)
    {
      modio::writeLogLine("Paginated response, retrieving next page...", MODIO_DEBUGLEVEL_LOG);
      ModioFilterCreator filter;
      modioInitFilter(&filter);
      for (auto &mod_id : *ptr_mod_ids)
      {
        modioAddFilterInField(&filter, "id", modio::toString(mod_id).c_str());
      }
      modioSetFilterOffset(&filter, response.result_offset + response.result_count);
      modioGetAllMods(ptr_mod_ids, filter, &onModsUpdateEvent);
      modioFreeFilter(&filter);
    }else
    {
      delete ptr_mod_ids;
    }
  }else
  {
    modio::writeLogLine("Could not retrive updated mods data", MODIO_DEBUGLEVEL_ERROR);
    delete ptr_mod_ids;
  }
}

void updateModsCache(std::vector<u32> mod_ids)
{
  ModioFilterCreator filter;
  modioInitFilter(&filter);

  std::vector<int>* ptr_mod_ids = new std::vector<int>();
  
  for (auto &mod_id : mod_ids)
  {
    modioAddFilterInField(&filter, "id", modio::toString(mod_id).c_str());
    ptr_mod_ids->push_back(mod_id);
  }

  modioGetAllMods(ptr_mod_ids, filter, &onModsUpdateEvent);
  modioFreeFilter(&filter);
}

void addModsToDownloadQueue(std::vector<u32> mod_ids)
{
  ModioFilterCreator filter;
  modioInitFilter(&filter);
  
  std::vector<int>* ptr_mod_ids = new std::vector<int>();
  
  for (auto &mod_id : mod_ids)
  {
    modioAddFilterInField(&filter, "id", modio::toString(mod_id).c_str());
    ptr_mod_ids->push_back(mod_id);
  }

  modioGetAllMods(ptr_mod_ids, filter, &modio::onAddModsToDownloadQueue);
  modioFreeFilter(&filter);
}

static void onGetAllEventsPoll(void *object, ModioResponse response, ModioModEvent *events_array, u32 events_array_size)
{
  u32* last_mod_event_poll_id_ptr = (u32*)object;
  if (response.code == 200)
  {
    modio::writeLogLine("Mod events polled", MODIO_DEBUGLEVEL_LOG);

    std::vector<u32> mod_edited_ids;
    std::vector<u32> mod_to_download_queue_ids;
    for (size_t i = 0; i < events_array_size; i++)
    {
      if(events_array[i].id > modio::LAST_MOD_EVENT_POLL_ID)
      {
        modio::writeLogLine("Mod event id polled: " + modio::toString(events_array[i].id), MODIO_DEBUGLEVEL_LOG);
        modio::LAST_MOD_EVENT_POLL_ID = events_array[i].id;
      }
      
      switch (events_array[i].event_type)
      {
      case MODIO_EVENT_UNDEFINED:
        // TODO: Log error
        break;
      case MODIO_EVENT_MODFILE_CHANGED:
      {
        bool reinstall = true;
        for (auto installed_mod : modio::installed_mods)
        {
          if (modio::hasKey(installed_mod, "mod_id") && modio::hasKey(installed_mod, "date_updated") && installed_mod["mod_id"] == events_array[i].mod_id && installed_mod["date_updated"] >= events_array[i].date_added)
          {
            reinstall = false;
            modio::writeLogLine("Modfile changed event detected but you already have a newer version installed, the modfile will not be downloaded. Mod id: " + modio::toString(events_array[i].mod_id), MODIO_DEBUGLEVEL_LOG);
          }
        }

        if (reinstall)
        {
          modio::writeLogLine("Modfile changed. Mod id: " + modio::toString(events_array[i].mod_id) + " Reisntalling...", MODIO_DEBUGLEVEL_LOG);
          mod_to_download_queue_ids.push_back(events_array[i].mod_id);
        }
      }
      break;
      case MODIO_EVENT_MOD_AVAILABLE:
      {
        // N/A
        break;
      }
      case MODIO_EVENT_MOD_UNAVAILABLE:
      {
        // N/A
        break;
      }
      case MODIO_EVENT_MOD_EDITED:
      {
        modio::writeLogLine("Mod updated. Mod id: " + modio::toString(events_array[i].mod_id) + " Updating cache...", MODIO_DEBUGLEVEL_LOG);
        mod_edited_ids.push_back(events_array[i].mod_id);
        break;
      }
      }
    }
    if (mod_edited_ids.size() > 0)
      updateModsCache(mod_edited_ids);
    if (mod_to_download_queue_ids.size() > 0)
      addModsToDownloadQueue(mod_to_download_queue_ids);

    nlohmann::json event_polling_json = modio::openJson(modio::getModIODirectory() + "event_polling.json");
    event_polling_json["last_mod_event_poll_id"] = modio::LAST_MOD_EVENT_POLL_ID;
    modio::writeJson(modio::getModIODirectory() + "event_polling.json", event_polling_json);

    if (modio::event_listener_callback && events_array_size > 0)
    {
      modio::writeLogLine("Triggering user callback listener", MODIO_DEBUGLEVEL_LOG);
      modio::event_listener_callback(response, events_array, events_array_size);
    }

    if(response.result_offset + response.result_count < response.result_total)
    {
      modio::writeLogLine("Paginated response, retrieving next page...", MODIO_DEBUGLEVEL_LOG);
      ModioFilterCreator filter;
      modioInitFilter(&filter);
      modioAddFilterGreaterThanField(&filter, "id", modio::toString(*last_mod_event_poll_id_ptr).c_str());

      for (auto installed_mod : modio::installed_mods)
      {
        if (modio::hasKey(installed_mod, "mod_id"))
          modioAddFilterInField(&filter, "mod_id", modio::toString((u32)installed_mod["mod_id"]).c_str());
      }
      modioSetFilterOffset(&filter, response.result_offset + response.result_count);
      modioGetAllEvents(last_mod_event_poll_id_ptr, filter, &onGetAllEventsPoll);
      modioFreeFilter(&filter);
    }else
    {
      delete last_mod_event_poll_id_ptr;
    }
  }
  else
  {
    modio::writeLogLine("Could not poll mod events. Error code: " + modio::toString(response.code), MODIO_DEBUGLEVEL_ERROR);
    delete last_mod_event_poll_id_ptr;
  }
}

static void onGetUserEventsPoll(void *object, ModioResponse response, ModioUserEvent *events_array, u32 events_array_size)
{
  u32* last_user_event_poll_id_ptr = (u32*)object;
  if (response.code == 200)
  {
    modio::writeLogLine("User events polled ", MODIO_DEBUGLEVEL_LOG);

    std::vector<u32> mod_to_download_queue_ids;
    for (size_t i = 0; i < events_array_size; i++)
    {
      if(events_array[i].id > modio::LAST_USER_EVENT_POLL_ID)
      {
        modio::writeLogLine("User event id polled: " + modio::toString(events_array[i].id), MODIO_DEBUGLEVEL_LOG);
        modio::LAST_USER_EVENT_POLL_ID = events_array[i].id;
      }

      switch (events_array[i].event_type)
      {
      case MODIO_EVENT_UNDEFINED:
        // TODO: Log error
        break;
      case MODIO_EVENT_USER_TEAM_JOIN:
      {
        // TODO: N/A
      }
      break;
      case MODIO_EVENT_USER_TEAM_LEAVE:
      {
        // N/A
        break;
      }
      case MODIO_EVENT_USER_SUBSCRIBE:
      {
        modio::writeLogLine("Current User subscribed to a Mod. Mod id: " + modio::toString(events_array[i].mod_id) + " Installing...", MODIO_DEBUGLEVEL_LOG);
        modio::current_user_subscriptions.insert(events_array[i].mod_id);
        std::string modfile_path_str = modio::getInstalledModPath(events_array[i].mod_id);
        if (modfile_path_str == "")
        {
          modio::writeLogLine("Installing mod. Id: " + modio::toString(events_array[i].mod_id), MODIO_DEBUGLEVEL_LOG);
          mod_to_download_queue_ids.push_back(events_array[i].mod_id);
        }
        break;
      }
      case MODIO_EVENT_USER_UNSUBSCRIBE:
      {
        modio::writeLogLine("Current User unsubscribed from a Mod. Mod id: " + modio::toString(events_array[i].mod_id) + " Uninstalling...", MODIO_DEBUGLEVEL_LOG);
        modio::current_user_subscriptions.erase(events_array[i].mod_id);
        modioUninstallMod(events_array[i].mod_id);
        break;
      }
      }
    }
    if (mod_to_download_queue_ids.size() > 0)
      addModsToDownloadQueue(mod_to_download_queue_ids);
    nlohmann::json token_json = modio::openJson(modio::getModIODirectory() + "authentication.json");
    token_json["last_user_event_poll_id"] = modio::LAST_USER_EVENT_POLL_ID;
    modio::writeJson(modio::getModIODirectory() + "authentication.json", token_json);

    if (modio::event_listener_callback && events_array_size > 0)
    {
      modio::writeLogLine("Triggering event callback listener", MODIO_DEBUGLEVEL_LOG);

      ModioModEvent *mod_events_array = new ModioModEvent[events_array_size];
      for(u32 i=0; i < events_array_size; i++)
      {
        mod_events_array[i].id = events_array[i].id;
        mod_events_array[i].mod_id = events_array[i].mod_id;
        mod_events_array[i].user_id = events_array[i].user_id;
        mod_events_array[i].event_type = events_array[i].event_type;
        mod_events_array[i].date_added = events_array[i].date_added;
      }

      modio::event_listener_callback(response, mod_events_array, events_array_size);

      for(u32 i=0; i < events_array_size; i++)
      {
        modioFreeModEvent(&mod_events_array[i]);
      }
      delete[] mod_events_array;
    }

    if(response.result_offset + response.result_count < response.result_total)
    {
      modio::writeLogLine("Paginated response, retrieving next pagee...", MODIO_DEBUGLEVEL_LOG);
      ModioFilterCreator filter;
      modio::writeLogLine("Last id: " + modio::toString(*last_user_event_poll_id_ptr), MODIO_DEBUGLEVEL_LOG);
      modioInitFilter(&filter);
      modioAddFilterGreaterThanField(&filter, "id", modio::toString(*last_user_event_poll_id_ptr).c_str());
      modioSetFilterOffset(&filter, response.result_offset + response.result_count);
      modioGetUserEvents(last_user_event_poll_id_ptr, filter, &onGetUserEventsPoll);
      modioFreeFilter(&filter);
    }else
    {
      delete last_user_event_poll_id_ptr;
    }
  }
  else
  {
    modio::writeLogLine("Could not poll user events. Error code: " + modio::toString(response.code), MODIO_DEBUGLEVEL_ERROR);
    delete last_user_event_poll_id_ptr;
  }
}

void pollEvents()
{
  u32 current_time = modio::getCurrentTimeSeconds();

  //if (current_time >= modio::RETRY_AFTER)
  //{
  if (modioGetAllInstalledModsCount() > 0 && current_time - modio::LAST_MOD_EVENT_POLL_TIME > modio::MOD_EVENT_POLL_INTERVAL)
  {
    modio::writeLogLine("Polling mod events", MODIO_DEBUGLEVEL_LOG);

    ModioFilterCreator filter;
    modioInitFilter(&filter);
    modioAddFilterGreaterThanField(&filter, "id", modio::toString(modio::LAST_MOD_EVENT_POLL_ID).c_str());

    for (auto installed_mod : modio::installed_mods)
    {
      if (modio::hasKey(installed_mod, "mod_id"))
        modioAddFilterInField(&filter, "mod_id", modio::toString((u32)installed_mod["mod_id"]).c_str());
    }

    u32* last_mod_event_poll_id_ptr = new u32;
    *last_mod_event_poll_id_ptr = modio::LAST_MOD_EVENT_POLL_ID;

    modioGetAllEvents(last_mod_event_poll_id_ptr, filter, &onGetAllEventsPoll);
    modioFreeFilter(&filter);

    modio::LAST_MOD_EVENT_POLL_TIME = current_time;
  }
  /*
  else if (current_time - modio::LAST_MOD_EVENT_POLL > modio::MOD_EVENT_POLL_INTERVAL)
  {
    nlohmann::json event_polling_json = modio::openJson(modio::getModIODirectory() + "event_polling.json");
    event_polling_json["last_mod_event_poll"] = current_time;
    modio::writeJson(modio::getModIODirectory() + "event_polling.json", event_polling_json);
  }
  */

  if (modioIsLoggedIn() && current_time - modio::LAST_USER_EVENT_POLL_TIME > modio::USER_EVENT_POLL_INTERVAL)
  {
    modio::writeLogLine("Polling user events", MODIO_DEBUGLEVEL_LOG);

    ModioFilterCreator filter;
    modioInitFilter(&filter);
    //modioAddFilterMinField(&filter, "date_added", modio::toString(modio::LAST_USER_EVENT_POLL).c_str());
    modioAddFilterGreaterThanField(&filter, "id", modio::toString(modio::LAST_USER_EVENT_POLL_ID).c_str());

    u32 *last_user_event_poll_id_ptr = new u32;
    *last_user_event_poll_id_ptr = (u32)modio::LAST_USER_EVENT_POLL_ID;

    modioGetUserEvents(last_user_event_poll_id_ptr, filter, &onGetUserEventsPoll);
    modioFreeFilter(&filter);

    modio::LAST_USER_EVENT_POLL_TIME = current_time;
  }
  //}
}

void updateAuthenticatedUser(std::string access_token)
{
  modio::writeLogLine("Updating authenticated user.", MODIO_DEBUGLEVEL_LOG);
  modio::ACCESS_TOKEN = access_token;
  nlohmann::json authentication_json;
  authentication_json["access_token"] = access_token;
  modio::writeJson(modio::getModIODirectory() + "authentication.json", authentication_json);
  modioGetAuthenticatedUser(NULL, &modio::onUpdateCurrentUser);
  modio::updateUserRatings();
  modio::updateUserSubscriptions();
}

void updateUserRatings()
{
  ModioFilterCreator filter;
  modioInitFilter(&filter);
  modioGetUserRatings(NULL, filter, &modio::onUpdateCurrentUserRatings);
  modioFreeFilter(&filter);
}

void updateUserSubscriptions()
{
  ModioFilterCreator filter;
  modioInitFilter(&filter);
  modioGetUserSubscriptions(NULL, filter, &modio::onUpdateCurrentUserSubscriptions);
  modioFreeFilter(&filter);
}

void addGameIdFilter(ModioFilterCreator &filter)
{
  if(!modio::RETRIEVE_MODS_FROM_OTHER_GAMES)
    modioAddFilterFieldValue(&filter, "game_id", modio::toString(modio::GAME_ID).c_str());
}

void handleDownloadImageError(void *object, void (*callback)(void *object, ModioResponse modioresponse))
{
  ModioResponse response;
  nlohmann::json empty_json;
  modioInitResponse(&response, empty_json);
  callback(object, response);
  modioFreeResponse(&response);
}

} // namespace modio
