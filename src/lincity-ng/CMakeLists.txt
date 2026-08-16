add_library(lincity_ng_lib STATIC
  ButtonPanel.cpp
  ButtonPanel.hpp
  Config.cpp
  Config.hpp
  Dialog.cpp
  Dialog.hpp
  EconomyGraph.cpp
  EconomyGraph.hpp
  Game.cpp
  Game.hpp
  GameView.cpp
  GameView.hpp
  HelpWindow.cpp
  HelpWindow.hpp
  Language.cpp
  MainLincity.cpp
  MainLincity.hpp
  MainMenu.cpp
  MainMenu.hpp
  MapPoint.hpp
  MapThumbnail.cpp
  MapThumbnail.hpp
  MiniMap.cpp
  MiniMap.hpp
  Mps.cpp
  Mps.hpp
  PBar.cpp
  PBar.hpp
  ScreenInterface.cpp
  Sound.cpp
  Sound.hpp
  TimerInterface.cpp
  TimerInterface.hpp
  UserOperation.cpp
  UserOperation.hpp
  Util.cpp
  Util.hpp
  Video.cpp
)

add_library(LinCity::ng_lib ALIAS lincity_ng_lib)

target_link_libraries(lincity_ng_lib
  PUBLIC
  LinCity::lib
  LinCity::gui
  $<IF:$<TARGET_EXISTS:SDL3::SDL3>,SDL3::SDL3,SDL3::SDL3-static>
  $<IF:$<TARGET_EXISTS:SDL3_mixer::SDL3_mixer>,SDL3_mixer::SDL3_mixer,SDL3_mixer::SDL3_mixer-static>
  PRIVATE
  cfgpath
  gettextutil
  $<IF:$<TARGET_EXISTS:SDL3_image::SDL3_image>,SDL3_image::SDL3_image,SDL3_image::SDL3_image-static>
  fmt::fmt
  xmlutil
  ptrutil
)

include_directories(.)

include(TranslateTarget)
translate_target(lincity_ng_lib lincity-ng.pot)

add_executable(lincity-ng
  main.cpp
  main.hpp
)

target_link_libraries(lincity-ng
  PRIVATE
  LinCity::ng_lib
  gettextutil
  $<TARGET_NAME_IF_EXISTS:SDL3::SDL3main>
)
