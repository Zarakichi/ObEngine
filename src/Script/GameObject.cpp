#include <Bindings/Bindings.hpp>
#include <Scene/Scene.hpp>
#include <Script/GameObject.hpp>
#include <Script/Script.hpp>
#include <Script/ViliLuaBridge.hpp>
#include <System/Loaders.hpp>
#include <Transform/Units.hpp>
#include <Transform/UnitVector.hpp>
#include <Triggers/Trigger.hpp>
#include <Triggers/TriggerDatabase.hpp>
#include <Utils/VectorUtils.hpp>

namespace obe
{
    namespace Script
    {
        KAGUYA_MEMBER_FUNCTION_OVERLOADS_WITH_SIGNATURE(useExternalTriggerProxy, GameObject, useExternalTrigger, 3, 4,
            void(GameObject::*)(std::string, std::string, std::string, std::string));

        void loadScrGameObject(GameObject* obj, kaguya::State* lua)
        {
            (*lua)["CPP_Import"] = &loadLibBridge;
            (*lua)["CPP_Hook"] = &loadHookBridge;
            loadScrGameObjectLib(lua);
            (*lua)["This"] = obj;
        }

        void loadScrGameObjectLib(kaguya::State* lua)
        {
            (*lua)["CPP_GameObject"].setClass(kaguya::UserdataMetatable<GameObject>()
                .addFunction("LevelSprite", &GameObject::getLevelSprite)
                .addFunction("Collider", &GameObject::getCollider)
                .addFunction("Animator", &GameObject::getAnimator)
                .addFunction("State", &GameObject::getScript)
                .addFunction("canCollide", &GameObject::canCollide)
                .addFunction("canClick", &GameObject::canClick)
                .addFunction("canDisplay", &GameObject::canDisplay)
                .addFunction("delete", &GameObject::deleteObject)
                .addFunction("exec", &GameObject::exec)
                .addFunction("getInitialised", &GameObject::getInitialised)
                .addFunction("getPriority", &GameObject::getPriority)
                .addFunction("getPublicKey", &GameObject::getPublicKey)
                .addFunction("useLocalTrigger", &GameObject::useLocalTrigger)
                .addFunction("useExternalTrigger", useExternalTriggerProxy())
                .addOverloadedFunctions("sendQuery",
                                        &GameObject::sendQuery<int>,
                                        &GameObject::sendQuery<float>,
                                        &GameObject::sendQuery<std::string>,
                                        &GameObject::sendQuery<bool>,
                                        &GameObject::sendQuery<std::vector<int>>,
                                        &GameObject::sendQuery<std::vector<float>>,
                                        &GameObject::sendQuery<std::vector<std::string>>,
                                        &GameObject::sendQuery<std::vector<bool>>,
                                        &GameObject::sendQuery<std::map<int, int>>,
                                        &GameObject::sendQuery<std::map<int, float>>,
                                        &GameObject::sendQuery<std::map<int, std::string>>,
                                        &GameObject::sendQuery<std::map<int, bool>>,
                                        &GameObject::sendQuery<std::map<std::string, int>>,
                                        &GameObject::sendQuery<std::map<std::string, float>>,
                                        &GameObject::sendQuery<std::map<std::string, std::string>>,
                                        &GameObject::sendQuery<std::map<std::string, bool>>
                )
                .addFunction("sendRequireArgument", &GameObject::sendRequireArgumentFromLua)
                .addFunction("setInitialised", &GameObject::setInitialised)
            );
        }

        void loadLibBridge(GameObject* object, std::string lib)
        {
            Bindings::Load(object->getScript(), lib);
        }

        void loadHookBridge(GameObject* object, std::string hookname)
        {
            loadHook(object->getScript(), hookname);
        }

        bool orderScrPriority(GameObject* g1, GameObject* g2)
        {
            return (g1->getPriority() > g2->getPriority());
        }


        //GameObjectRequires
        GameObjectRequires* GameObjectRequires::instance = nullptr;

        GameObjectRequires* GameObjectRequires::getInstance()
        {
            if (!instance)
                instance = new GameObjectRequires();
            return instance;
        }

        vili::ComplexNode* GameObjectRequires::getRequiresForObjectType(std::string type) const
        {
            if (!Utils::Vector::isInList(type, allRequires->getAll(vili::NodeType::ComplexNode)))
            {
                vili::ViliParser getGameObjectFile;
                System::Path("Data/GameObjects/").add(type).add(type + ".obj.vili").loadResource(&getGameObjectFile, System::Loaders::dataLoader);
                if (getGameObjectFile->contains("Requires"))
                {
                    vili::ComplexNode& requiresData = getGameObjectFile.at<vili::ComplexNode>("Requires");
                    getGameObjectFile->extractElement(&getGameObjectFile.at<vili::ComplexNode>("Requires"));
                    requiresData.setId(type);
                    allRequires->pushComplexNode(&requiresData);
                    return &requiresData;
                }
                return nullptr;
            }
            return &allRequires.at(type);
        }

        void GameObjectRequires::ApplyRequirements(GameObject* obj, vili::ComplexNode& requires)
        {
            for (std::string currentRequirement : requires.getAll())
            {
                requires.setId("Lua_ReqList");
                kaguya::LuaTable requireTable = ((*obj->getScript())["LuaCore"]);
                DataBridge::complexAttributeToLuaTable(requireTable, requires);
            }
        }

        //GameObject
        GameObject::GameObject(const std::string& type, const std::string& id) : Identifiable(id), m_localTriggers(nullptr)
        {
            m_type = type;
            m_id = id;
        }

        GameObject::~GameObject()
        {
            Triggers::TriggerDatabase::GetInstance()->removeNamespace(m_privateKey);
            Triggers::TriggerDatabase::GetInstance()->removeNamespace(m_publicKey);
        }

        void GameObject::registerTrigger(Triggers::Trigger* trg)
        {
            m_registeredTriggers.push_back(trg);
        }

        void GameObject::loadGameObject(Scene::Scene& world, vili::ComplexNode& obj)
        {
            //Animator
            std::string animatorPath;
            if (obj.contains(vili::NodeType::ComplexNode, "Animator"))
            {
                m_objectAnimator = std::make_unique<Animation::Animator>();
                animatorPath = obj.at("Animator").getDataNode("path").get<std::string>();
                if (animatorPath != "")
                {
                    m_objectAnimator->setPath(animatorPath);
                    m_objectAnimator->loadAnimator();
                }
                if (obj.at("Animator").contains(vili::NodeType::DataNode, "default"))
                {
                    m_objectAnimator->setKey(obj.at("Animator").getDataNode("default").get<std::string>());
                }
                m_hasAnimator = true;
            }
            //Collider
            if (obj.contains(vili::NodeType::ComplexNode, "Collider"))
            {
                m_objectCollider = world.createCollider(m_id);

                std::string pointsUnit = obj.at("Collider", "unit").getDataNode("unit").get<std::string>();
                bool completePoint = true;
                double pointBuffer;
                Transform::Units pBaseUnit = Transform::stringToUnits(pointsUnit);
                for (vili::DataNode* colliderPoint : obj.at("Collider").getArrayNode("points"))
                {
                    if (completePoint = !completePoint)
                    {
                        Transform::UnitVector pVector2 = Transform::UnitVector(
                            pointBuffer,
                            colliderPoint->get<double>(),
                            pBaseUnit
                        ).to<Transform::Units::WorldPixels>();
                        m_objectCollider->addPoint(pVector2);
                    }
                    else
                        pointBuffer = colliderPoint->get<double>();
                }
                if (obj.at("Collider").contains(vili::NodeType::DataNode, "tag"))
                    m_objectCollider->addTag(Collision::ColliderTagType::Tag, obj.at<vili::DataNode>("Collider", "tag").get<std::string>());
                else if (obj.at("Collider").contains(vili::NodeType::ArrayNode, "tags"))
                {
                    for (vili::DataNode* cTag : obj.at<vili::ArrayNode>("Collider", "tags"))
                        m_objectCollider->addTag(Collision::ColliderTagType::Tag, cTag->get<std::string>());
                }
                if (obj.at("Collider").contains(vili::NodeType::DataNode, "accept"))
                    m_objectCollider->addTag(Collision::ColliderTagType::Accepted, obj.at<vili::DataNode>("Collider", "accept").get<std::string>());
                else if (obj.at("Collider").contains(vili::NodeType::ArrayNode, "accept"))
                {
                    for (vili::DataNode* aTag : obj.at<vili::ArrayNode>("Collider", "accept"))
                        m_objectCollider->addTag(Collision::ColliderTagType::Accepted, aTag->get<std::string>());
                }
                if (obj.at("Collider").contains(vili::NodeType::DataNode, "reject"))
                    m_objectCollider->addTag(Collision::ColliderTagType::Rejected, obj.at<vili::DataNode>("Collider", "reject").get<std::string>());
                else if (obj.at("Collider").contains(vili::NodeType::ArrayNode, "reject"))
                {
                    for (vili::DataNode* rTag : obj.at<vili::ArrayNode>("Collider", "reject"))
                        m_objectCollider->addTag(Collision::ColliderTagType::Rejected, rTag->get<std::string>());
                }

                m_hasCollider = true;
            }
            //LevelSprite
            if (obj.contains(vili::NodeType::ComplexNode, "LevelSprite"))
            {
                m_objectLevelSprite = world.createLevelSprite(m_id);
                m_levelSpriteRelative = (obj.at("LevelSprite").getDataNode("position").get<std::string>() == "relative") ? true : false;
                int sprOffX = 0;
                int sprOffY = 0;
                std::string spriteXTransformer;
                std::string spriteYTransformer;
                vili::ComplexNode& currentSprite = obj.at("LevelSprite");

                std::string spritePath = currentSprite.contains(vili::NodeType::DataNode, "path") ?
                                             currentSprite.getDataNode("path").get<std::string>() : "";
                Transform::UnitVector spritePos = Transform::UnitVector(
                    currentSprite.contains(vili::NodeType::ComplexNode, "pos") ?
                        currentSprite.at<vili::DataNode>("pos", "x").get<double>() : 0,
                    currentSprite.contains(vili::NodeType::ComplexNode, "pos") ?
                        currentSprite.at<vili::DataNode>("pos", "y").get<double>() : 0
                );
                int spriteRot = currentSprite.contains(vili::NodeType::DataNode, "rotation") ?
                                    currentSprite.getDataNode("rotation").get<int>() : 0;
                int layer = currentSprite.contains(vili::NodeType::DataNode, "layer") ?
                                currentSprite.getDataNode("layer").get<int>() : 1;
                int zdepth = currentSprite.contains(vili::NodeType::DataNode, "z-depth") ?
                                 currentSprite.getDataNode("z-depth").get<int>() : 1;

                if (currentSprite.contains(vili::NodeType::DataNode, "xTransform"))
                {
                    spriteXTransformer = currentSprite.at<vili::DataNode>("xTransform").get<std::string>();
                }
                else
                {
                    spriteXTransformer = "None";
                }
                if (currentSprite.contains(vili::NodeType::DataNode, "yTransform"))
                {
                    spriteYTransformer = currentSprite.at<vili::DataNode>("yTransform").get<std::string>();
                }
                else
                {
                    spriteYTransformer = "None";
                }

                m_objectLevelSprite->load(spritePath);
                m_objectLevelSprite->setPosition(spritePos.x, spritePos.y);
                m_objectLevelSprite->setRotation(spriteRot);
                //ADD SPRITE SIZE
                Graphics::PositionTransformers::PositionTransformer positionTransformer(spriteXTransformer, spriteYTransformer);
                m_objectLevelSprite->setPositionTransformer(positionTransformer);
                m_objectLevelSprite->setLayer(layer);
                m_objectLevelSprite->setZDepth(zdepth);
                m_hasLevelSprite = true;
                world.reorganizeLayers();
            }
            //Script
            if (obj.contains(vili::NodeType::ComplexNode, "Script"))
            {
                m_objectScript = std::make_unique<kaguya::State>();
                m_hasScriptEngine = true;
                m_objectScript = std::make_unique<kaguya::State>();
                m_privateKey = Utils::String::getRandomKey("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789", 12);
                m_publicKey = Utils::String::getRandomKey("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789", 12);
                Triggers::TriggerDatabase::GetInstance()->createNamespace(m_privateKey);
                Triggers::TriggerDatabase::GetInstance()->createNamespace(m_publicKey);
                m_localTriggers = Triggers::TriggerDatabase::GetInstance()->createTriggerGroup(m_privateKey, "Local");

                System::Path("Lib/Internal/ScriptInit.lua").loadResource(m_objectScript.get(), System::Loaders::luaLoader);
                loadScrGameObject(this, m_objectScript.get());

                m_localTriggers
                    ->addTrigger("Init")
                    ->addTrigger("Update")
                    ->setPermanent("Update", true)
                    ->trigger("Update")
                    ->addTrigger("Query")
                    ->addTrigger("Collide")
                    ->addTrigger("Click")
                    ->addTrigger("Press")
                    ->addTrigger("Delete")
                    ->addTrigger("Save");

                System::Path("Lib/Internal/ObjectInit.lua").loadResource(m_objectScript.get(), System::Loaders::luaLoader);

                (*m_objectScript)["Private"] = m_privateKey;
                (*m_objectScript)["Public"] = m_publicKey;

                if (obj.at("Script").contains(vili::NodeType::DataNode, "source"))
                {
                    std::string getScrName = obj.at("Script").getDataNode("source").get<std::string>();
                    System::Path(getScrName).loadResource(m_objectScript.get(), System::Loaders::luaLoader);
                }
                else if (obj.at("Script").contains(vili::NodeType::ArrayNode, "sources"))
                {
                    int scriptListSize = obj.at("Script").getArrayNode("sources").size();
                    for (int i = 0; i < scriptListSize; i++)
                    {
                        std::string getScrName = obj.at("Script").getArrayNode("sources").get(i).get<std::string>();
                        System::Path(getScrName).loadResource(m_objectScript.get(), System::Loaders::luaLoader);
                    }
                }
                if (obj.at("Script").contains(vili::NodeType::DataNode, "priority"))
                    m_scrPriority = obj.at("Script").getDataNode("priority").get<int>();

                m_localTriggers->pushParameter("Init", "Lol", 3);
                m_localTriggers->pushParameter("Init", "Mdr", 10);
                m_localTriggers->trigger("Init");
                m_localTriggers->pushParameter("Init", "Lol", 22);
                m_localTriggers->pushParameter("Init", "Mdr", 44);
                m_localTriggers->trigger("Init");
            }
        }

        void GameObject::update(double dt)
        {
            if (m_updated)
            {
                for (int i = 0; i < m_registeredTriggers.size(); i++)
                {
                    Triggers::Trigger* trigger = m_registeredTriggers[i];
                    if (trigger->getState())
                    {
                        std::string useGrp = trigger->getGroup();
                        for (int j = 0; j < m_registeredAliases.size(); j++)
                        {
                            std::string alNsp, alGrp, alRep;
                            tie(alNsp, alGrp, alRep) = m_registeredAliases[j];
                            if (alNsp == trigger->getNamespace() && alGrp == trigger->getGroup())
                                useGrp = alRep;
                        }
                        std::string funcname = useGrp + "." + trigger->getName();
                        if (funcname == "Local.Update")
                        {
                            m_localTriggers->pushParameter("Update", "dt", dt);
                        }
                        trigger->execute(m_objectScript.get(), funcname);
                        if (funcname == "Local.Init")
                        {
                            this->setInitialised(true);
                        }
                    }
                }
                if (m_initialised)
                {
                    if (m_hasAnimator)
                    {
                        m_objectAnimator->update();
                        if (m_hasLevelSprite)
                        {
                            m_objectLevelSprite->setTexture(m_objectAnimator->getTexture());
                        }
                    }
                }
            }
        }

        std::string GameObject::getType() const
        {
            return m_type;
        }

        std::string GameObject::getPublicKey() const
        {
            return m_publicKey;
        }

        int GameObject::getPriority() const
        {
            return m_scrPriority;
        }

        bool GameObject::canDisplay() const
        {
            return m_hasLevelSprite;
        }

        bool GameObject::canCollide() const
        {
            return (m_hasCollider && m_colliderSolid);
        }

        bool GameObject::canClick() const
        {
            return (m_hasCollider && m_colliderClick);
        }

        bool GameObject::doesHaveAnimator() const
        {
            return m_hasAnimator;
        }

        bool GameObject::doesHaveCollider() const
        {
            return m_hasCollider;
        }

        bool GameObject::doesHaveLevelSprite() const
        {
            return m_hasLevelSprite;
        }

        bool GameObject::doesHaveScriptEngine() const
        {
            return m_hasScriptEngine;
        }

        bool GameObject::isLevelSpriteRelative() const
        {
            return m_levelSpriteRelative;
        }

        bool GameObject::getUpdateState() const
        {
            return m_updated;
        }

        void GameObject::setUpdateState(bool state)
        {
            m_updated = state;
        }

        Graphics::LevelSprite* GameObject::getLevelSprite()
        {
            if (m_hasLevelSprite)
                return m_objectLevelSprite;
            throw aube::ErrorHandler::Raise("ObEngine.GameObject.GameObject.NoLevelSprite", {{"id", m_id}});
        }

        Collision::PolygonalCollider* GameObject::getCollider()
        {
            if (m_hasCollider)
                return m_objectCollider;
            throw aube::ErrorHandler::Raise("ObEngine.GameObject.GameObject.NoCollider", {{"id", m_id}});
        }

        Animation::Animator* GameObject::getAnimator()
        {
            if (m_hasAnimator)
                return m_objectAnimator.get();
            throw aube::ErrorHandler::Raise("ObEngine.GameObject.GameObject.NoAnimator", {{"id", m_id}});
        }

        kaguya::State* GameObject::getScript()
        {
            if (m_hasScriptEngine)
                return m_objectScript.get();
            throw aube::ErrorHandler::Raise("ObEngine.GameObject.GameObject.NoScript", {{"id", m_id}});
        }

        Triggers::TriggerGroup* GameObject::getLocalTriggers() const
        {
            return m_localTriggers.operator->();
        }

        void GameObject::useLocalTrigger(const std::string& trName)
        {
            this->registerTrigger(Triggers::TriggerDatabase::GetInstance()->getTrigger(m_privateKey, "Local", trName));
            Triggers::TriggerDatabase::GetInstance()->getTrigger(m_privateKey, "Local", trName)->registerState(m_objectScript.get());
        }

        void GameObject::useExternalTrigger(const std::string& trNsp, const std::string& trGrp, const std::string& trName, const std::string& useAs)
        {
            //std::cout << "REGISTERING ET : " << trNsp << ", " << trGrp << ", " << trName << ", " << useAs << std::endl;
            if (trName == "*")
            {
                std::vector<std::string> allEv = Triggers::TriggerDatabase::GetInstance()->getAllTriggersNameFromTriggerGroup(trNsp, trGrp);
                for (int i = 0; i < allEv.size(); i++)
                {
                    if (!Utils::Vector::isInList(Triggers::TriggerDatabase::GetInstance()->getTrigger(trNsp, trGrp, allEv[i]), m_registeredTriggers))
                    {
                        this->registerTrigger(Triggers::TriggerDatabase::GetInstance()->getTrigger(trNsp, trGrp, allEv[i]));
                        Triggers::TriggerDatabase::GetInstance()->getTrigger(trNsp, trGrp, allEv[i])->registerState(m_objectScript.get());
                    }
                }
            }
            else 
            {
                //std::cout << "Registering Single Trigger" << std::endl;
                if (!Utils::Vector::isInList(Triggers::TriggerDatabase::GetInstance()->getTrigger(trNsp, trGrp, trName), m_registeredTriggers))
                {
                    this->registerTrigger(Triggers::TriggerDatabase::GetInstance()->getTrigger(trNsp, trGrp, trName));
                    Triggers::TriggerDatabase::GetInstance()->getTrigger(trNsp, trGrp, trName)->registerState(m_objectScript.get());
                }
            }
            if (useAs != "")
            {
                m_registeredAliases.push_back(std::tuple<std::string, std::string, std::string>(trNsp, trGrp, useAs));
            }
        }

        void GameObject::setInitialised(bool init)
        {
            m_initialised = init;
        }

        bool GameObject::getInitialised() const
        {
            return m_initialised;
        }

        void GameObject::exec(std::string query) const
        {
            m_objectScript->dostring(query);
        }

        void GameObject::sendRequireArgumentFromLua(const std::string& argName, kaguya::LuaRef value) const
        {
            (*m_objectScript)["LuaCore"]["Lua_ReqList"][argName] = value;
        }

        void GameObject::deleteObject()
        {
            this->deletable = true;
        }
    }
}