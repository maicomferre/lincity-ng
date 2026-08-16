set(guiDialogSourceFiles
  coal_survey_yn.xml
  governor.xml
  gamestats.xml
  launch_rocket_yn.xml
  portdialog.xml
  tradedialog.xml
)

set(xslFiles dialogs.xsl)

foreach(guiDialogSourceFile ${guiDialogSourceFiles})
  if(ENABLE_NLS)
    set(XML_NLS enabled)
  else()
    set(XML_NLS disabled)
  endif()
  add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${guiDialogSourceFile}
    COMMAND LibXslt::xsltproc -o ${CMAKE_CURRENT_BINARY_DIR}/${guiDialogSourceFile} ${guiDialogSourceFile}
      --stringparam NLS ${XML_NLS}
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${guiDialogSourceFile} ${xslFiles}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Converting dialogs/${guiDialogSourceFile}"
  )

  install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${guiDialogSourceFile} DESTINATION ${CMAKE_INSTALL_APPDATADIR}/gui/dialogs/)
endforeach()

add_custom_target(guiDialogXml DEPENDS ${guiDialogSourceFiles})
add_dependencies(lincity-ng guiDialogXml)

# Extract the translatable strings from the TRANSFORMED dialog files:
# dialogs.xsl is what turns the <title> into a translatable Paragraph, so
# the pot is generated from the xsltproc output. (guiXml.pot covers only
# data/gui/*.xml, not this directory.)
set(GUI_DIALOG_POT ${CMAKE_CURRENT_BINARY_DIR}/guiDialogXml.pot)
set(_dialog_transformed)
foreach(guiDialogSourceFile ${guiDialogSourceFiles})
  list(APPEND _dialog_transformed
    ${CMAKE_CURRENT_BINARY_DIR}/${guiDialogSourceFile})
endforeach()
add_custom_command(
  OUTPUT ${GUI_DIALOG_POT}
  WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
  COMMAND ${GETTEXT_XGETTEXT} --from-code=UTF-8 --its=${CMAKE_CURRENT_SOURCE_DIR}/../gui.its
    -o ${GUI_DIALOG_POT} ${_dialog_transformed}
  DEPENDS ${guiDialogSourceFiles} ${CMAKE_CURRENT_SOURCE_DIR}/../gui.its
  COMMENT "generating guiDialogXml.pot"
)
add_custom_target(guiDialogXml.pot.target DEPENDS ${GUI_DIALOG_POT})
