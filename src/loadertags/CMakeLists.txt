set_source_files_properties(blockcontext.h
  PROPERTIES SKIP_AUTOMOC TRUE
)

add_library(ktexttemplate_loadertags MODULE
  loadertags.cpp
  blockcontext.cpp
  block.cpp
  extends.cpp
  include.cpp
)
set_property(TARGET ktexttemplate_loadertags PROPERTY
  EXPORT_NAME loadertags
)
target_link_libraries(ktexttemplate_loadertags PRIVATE
  KF6::TextTemplate
)
ktexttemplate_adjust_plugin_name(ktexttemplate_loadertags)

install(TARGETS ktexttemplate_loadertags
  EXPORT ktexttemplate_targets
  LIBRARY DESTINATION ${PLUGIN_INSTALL_DIR}
  COMPONENT Templates
)
