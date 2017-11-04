#include <Bindings/SystemBindings.hpp>
#include <System/Cursor.hpp>
#include <System/Loaders.hpp>
#include <System/Package.hpp>
#include <System/Path.hpp>
#include <System/Workspace.hpp>
#include <Transform/UnitVector.hpp>
#include <System/Window.hpp>

namespace obe
{
    namespace Bindings
    {
        namespace SystemBindings
        {

            std::string getOsName()
            {
                #ifdef _WIN32
                return "Windows 32-bit";
                #elif _WIN64
                return "Windows 64-bit";
                #elif __unix || __unix__
                return "Unix";
                #elif __APPLE__ || __MACH__
                return "Mac OSX";
                #elif __linux__
                return "Linux";
                #elif __FreeBSD__
                return "FreeBSD";
                #else
                return "Other";
                #endif
            }
            void LoadSystemConstants(kaguya::State* lua)
            {
                (*lua)["obe"]["OS"] = getOsName();
                (*lua)["obe"]["Screen"] = kaguya::NewTable();
                (*lua)["obe"]["Screen"]["Width"] = Transform::UnitVector::Screen.w;
                (*lua)["obe"]["Screen"]["Height"] = Transform::UnitVector::Screen.h;
            }

            void LoadSCursor(kaguya::State* lua)
            {
                (*lua)["obe"]["Cursor"].setClass(kaguya::UserdataMetatable<System::Cursor>()
                    .addFunction("getRawX", &System::Cursor::getX)
                    .addFunction("getRawY", &System::Cursor::getY)
                    .addFunction("getX", &System::Cursor::getConstrainedX)
                    .addFunction("getY", &System::Cursor::getConstrainedY)
                    .addFunction("setConstraint", &System::Cursor::setConstraint)
                    .addFunction("setPosition", &System::Cursor::setPosition)
                    .addFunction("setX", &System::Cursor::setX)
                    .addFunction("setY", &System::Cursor::setY)
                    .addFunction("update", &System::Cursor::update)
                );
            }
            void LoadMountablePath(kaguya::State* lua)
            {
                (*lua)["obe"]["MountablePath"].setClass(kaguya::UserdataMetatable<System::MountablePath>()
                    .addProperty("basePath", &System::MountablePath::basePath)
                    .addProperty("priority", &System::MountablePath::priority)
                    .addProperty("pathType", &System::MountablePath::pathType)
                );
                (*lua)["obe"]["MountPaths"] = kaguya::function(System::MountPaths);
            }

            KAGUYA_MEMBER_FUNCTION_OVERLOADS_WITH_SIGNATURE(
                System_find_proxy, System::Path, find, 0, 1, std::string(System::Path::*)(System::PathType)
            );
            void LoadPath(kaguya::State* lua)
            {
                (*lua)["obe"]["Path"].setClass(kaguya::UserdataMetatable<System::Path>()
                    .setConstructors<System::Path(), System::Path(const std::string&)>()
                    .addStaticFunction("Mount", &System::Path::Mount)
                    .addStaticFunction("Paths", &System::Path::Paths)
                    .addFunction("add", &System::Path::add)
                    .addFunction("find", System_find_proxy())
                    .addFunction("getPath", &System::Path::getPath)
                    .addFunction("last", &System::Path::last)
                    .addFunction("toString", &System::Path::toString)
                );
                (*lua)["obe"]["Path"]["FileListLoader"] = kaguya::function([](const System::Path& path)
                {
                    std::vector<std::string> fileList;
                    path.loadResource(&fileList, System::Loaders::filePathLoader);
                    return fileList;
                });
                (*lua)["obe"]["Path"]["DirectoryListLoader"] = kaguya::function([](const System::Path& path)
                {
                    std::vector<std::string> dirList;
                    path.loadResource(&dirList, System::Loaders::dirPathLoader);
                    return dirList;
                });
                (*lua)["obe"]["Path"]["DataLoader"] = kaguya::function([](const System::Path& path)
                {
                    vili::ViliParser parsedFile;
                    path.loadResource(&parsedFile, System::Loaders::dataLoader);
                    return parsedFile;
                });
                (*lua)["obe"]["Path"]["FontLoader"] = kaguya::function([](const System::Path& path)
                {
                    sf::Font loadedFont;
                    path.loadResource(&loadedFont, System::Loaders::fontLoader);
                    return loadedFont;
                });
                (*lua)["obe"]["Path"]["LuaLoader"] = kaguya::function([](const System::Path& path, kaguya::State* lua)
                {
                    path.loadResource(lua, System::Loaders::luaLoader);
                });
                (*lua)["obe"]["PathType"] = kaguya::NewTable();
                (*lua)["obe"]["PathType"]["All"] = System::PathType::All;
                (*lua)["obe"]["PathType"]["Directory"] = System::PathType::Directory;
                (*lua)["obe"]["PathType"]["File"] = System::PathType::File;
                // Add missing loaders <REVISION>
            }

            void LoadPackage(kaguya::State* lua)
            {
                (*lua)["obe"]["Package"] = kaguya::NewTable();
                (*lua)["obe"]["Package"]["GetPackageLocation"] = kaguya::function(System::Package::GetPackageLocation);
                (*lua)["obe"]["Package"]["PackageExists"] = kaguya::function(System::Package::PackageExists);
                (*lua)["obe"]["Package"]["Install"] = kaguya::function(System::Package::Install);
                (*lua)["obe"]["Package"]["Load"] = kaguya::function(System::Package::Load);
            }

            void LoadWorkspace(kaguya::State* lua)
            {
                (*lua)["obe"]["Workspace"] = kaguya::NewTable();
                (*lua)["obe"]["Workspace"]["GetWorkspaceLocation"] = kaguya::function(System::Workspace::GetWorkspaceLocation);
                (*lua)["obe"]["Workspace"]["WorkspaceExists"] = kaguya::function(System::Workspace::WorkspaceExists);
                (*lua)["obe"]["Workspace"]["Load"] = kaguya::function(System::Workspace::Load);
            }

            void LoadWindow(kaguya::State* lua)
            {
                (*lua)["obe"]["Window"] = kaguya::NewTable();
                (*lua)["obe"]["Window"]["setTitle"] = kaguya::function(System::setTitle);
                (*lua)["obe"]["Window"]["setSize"] = kaguya::function(System::setSize);
            }
        }
    }
}
