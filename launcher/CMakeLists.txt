include(CSharpUtilities)

configure_file(
	${CMAKE_CURRENT_SOURCE_DIR}/ns_icon.ico
	${CMAKE_CURRENT_BINARY_DIR}/ns_icon.ico
	COPYONLY
)

add_executable(
	NorthstarLauncher
	WIN32
	App.xaml
	App.xaml.cs
	MainWindow.xaml
	MainWindow.xaml.cs
	ns_icon.ico
	AssemblyInfo.cs
	Models/ValidationResult.cs
	Services/LauncherInstallValidator.cs
	Services/NativeMethods.cs
	Services/GameLaunchService.cs
)

set_property(TARGET NorthstarLauncher PROPERTY DOTNET_SDK "Microsoft.NET.Sdk")
set_property(TARGET NorthstarLauncher PROPERTY DOTNET_TARGET_FRAMEWORK "net10.0-windows")
set_property(TARGET NorthstarLauncher PROPERTY VS_GLOBAL_UseWPF "true")
set_property(TARGET NorthstarLauncher PROPERTY VS_GLOBAL_Nullable "enable")
set_property(TARGET NorthstarLauncher PROPERTY VS_GLOBAL_ImplicitUsings "enable")
set_property(TARGET NorthstarLauncher PROPERTY VS_GLOBAL_AllowUnsafeBlocks "true")
set_property(TARGET NorthstarLauncher PROPERTY VS_GLOBAL_ApplicationIcon "ns_icon.ico")
set_property(TARGET NorthstarLauncher PROPERTY VS_GLOBAL_AssemblyTitle "Northstar Launcher")
set_property(TARGET NorthstarLauncher PROPERTY VS_GLOBAL_Company "Northstar Developers")
set_property(TARGET NorthstarLauncher PROPERTY VS_GLOBAL_Description "Northstar Launcher")
set_property(TARGET NorthstarLauncher PROPERTY VS_GLOBAL_Product "Northstar Launcher")
set_property(TARGET NorthstarLauncher PROPERTY VS_GLOBAL_Copyright "Copyright (C) 2021")

set_target_properties(
	NorthstarLauncher
	PROPERTIES
	RUNTIME_OUTPUT_DIRECTORY "${NS_BINARY_DIR}"
	RUNTIME_OUTPUT_DIRECTORY_DEBUG "${NS_BINARY_DIR}"
	RUNTIME_OUTPUT_DIRECTORY_RELEASE "${NS_BINARY_DIR}"
	RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${NS_BINARY_DIR}"
	RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${NS_BINARY_DIR}"
)

set_source_files_properties(
	App.xaml
	PROPERTIES
	VS_XAML_TYPE "ApplicationDefinition"
)

set_source_files_properties(
	MainWindow.xaml
	PROPERTIES
	VS_XAML_TYPE "Page"
)

set_source_files_properties(
	App.xaml.cs
	PROPERTIES
	VS_CSHARP_DependentUpon "App.xaml"
)

set_source_files_properties(
	MainWindow.xaml.cs
	PROPERTIES
	VS_CSHARP_DependentUpon "MainWindow.xaml"
)
