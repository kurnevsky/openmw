#include "shadow.hpp"

#include <osgShadow/ShadowedScene>
#include <osg/CullFace>
#include <osg/Geode>
#include <osg/io_utils>
#include <osgDB/FileUtils>
#include <osgDB/ReadFile>

#include <components/settings/settings.hpp>

namespace SceneUtil
{
    using namespace osgShadow;

    void ShadowManager::setupShadowSettings()
    {
        mEnableShadows = Settings::Manager::getBool("enable shadows", "Shadows");

        if (!mEnableShadows)
        {
            mShadowTechnique->disableShadows();
            return;
        }
        
        mShadowTechnique->enableShadows();

        mShadowSettings->setLightNum(0);
        mShadowSettings->setReceivesShadowTraversalMask(~0u);

        int numberOfShadowMapsPerLight = Settings::Manager::getInt("number of shadow maps", "Shadows");
        mShadowSettings->setNumShadowMapsPerLight(numberOfShadowMapsPerLight);
        mShadowSettings->setBaseShadowTextureUnit(8 - numberOfShadowMapsPerLight);

        mShadowSettings->setMinimumShadowMapNearFarRatio(Settings::Manager::getFloat("minimum lispsm near far ratio", "Shadows"));
        if (Settings::Manager::getBool("compute tight scene bounds", "Shadows"))
            mShadowSettings->setComputeNearFarModeOverride(osg::CullSettings::COMPUTE_NEAR_FAR_USING_PRIMITIVES);

        int mapres = Settings::Manager::getInt("shadow map resolution", "Shadows");
        mShadowSettings->setTextureSize(osg::Vec2s(mapres, mapres));

        mShadowTechnique->setSplitPointUniformLogarithmicRatio(Settings::Manager::getFloat("split point uniform logarithmic ratio", "Shadows"));
        mShadowTechnique->setSplitPointDeltaBias(Settings::Manager::getFloat("split point bias", "Shadows"));

        if (Settings::Manager::getBool("allow shadow map overlap", "Shadows"))
            mShadowSettings->setMultipleShadowMapHint(osgShadow::ShadowSettings::CASCADED);
        else
            mShadowSettings->setMultipleShadowMapHint(osgShadow::ShadowSettings::PARALLEL_SPLIT);

        if (Settings::Manager::getBool("enable debug hud", "Shadows"))
            mShadowTechnique->enableDebugHUD();
        else
            mShadowTechnique->disableDebugHUD();
    }

    void ShadowManager::disableShadowsForStateSet(osg::ref_ptr<osg::StateSet> stateset)
    {
        int numberOfShadowMapsPerLight = Settings::Manager::getInt("number of shadow maps", "Shadows");
        int baseShadowTextureUnit = 8 - numberOfShadowMapsPerLight;
        
        osg::ref_ptr<osg::Image> fakeShadowMapImage = new osg::Image();
        fakeShadowMapImage->allocateImage(1, 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT);
        *(float*)fakeShadowMapImage->data() = std::numeric_limits<float>::infinity();
        osg::ref_ptr<osg::Texture> fakeShadowMapTexture = new osg::Texture2D(fakeShadowMapImage);
        fakeShadowMapTexture->setShadowComparison(true);
        fakeShadowMapTexture->setShadowCompareFunc(osg::Texture::ShadowCompareFunc::ALWAYS);
        for (int i = baseShadowTextureUnit; i < baseShadowTextureUnit + numberOfShadowMapsPerLight; ++i)
            stateset->setTextureAttributeAndModes(i, fakeShadowMapTexture, osg::StateAttribute::ON | osg::StateAttribute::OVERRIDE | osg::StateAttribute::PROTECTED);
    }

    ShadowManager::ShadowManager(osg::ref_ptr<osg::Group> sceneRoot, osg::ref_ptr<osg::Group> rootNode, unsigned int outdoorShadowCastingMask, unsigned int indoorShadowCastingMask) : mShadowedScene(new osgShadow::ShadowedScene),
        mShadowTechnique(new MWShadowTechnique),
        mOutdoorShadowCastingMask(outdoorShadowCastingMask),
        mIndoorShadowCastingMask(indoorShadowCastingMask)
    {
        mShadowedScene->setShadowTechnique(mShadowTechnique);

        mShadowedScene->addChild(sceneRoot);
        rootNode->addChild(mShadowedScene);

        mShadowSettings = mShadowedScene->getShadowSettings();
        setupShadowSettings();

        enableOutdoorMode();
    }

    Shader::ShaderManager::DefineMap ShadowManager::getShadowDefines()
    {
        if (!mEnableShadows)
            return getShadowsDisabledDefines();

        Shader::ShaderManager::DefineMap definesWithShadows;
        definesWithShadows.insert(std::make_pair(std::string("shadows_enabled"), std::string("1")));
        for (unsigned int i = 0; i < mShadowSettings->getNumShadowMapsPerLight(); ++i)
            definesWithShadows["shadow_texture_unit_list"] += std::to_string(i) + ",";
        // remove extra comma
        definesWithShadows["shadow_texture_unit_list"] = definesWithShadows["shadow_texture_unit_list"].substr(0, definesWithShadows["shadow_texture_unit_list"].length() - 1);

        if (Settings::Manager::getBool("allow shadow map overlap", "Shadows"))
            definesWithShadows["shadowMapsOverlap"] = "1";
        else
            definesWithShadows["shadowMapsOverlap"] = "0";

        if (Settings::Manager::getBool("enable debug overlay", "Shadows"))
            definesWithShadows["useShadowDebugOverlay"] = "1";
        else
            definesWithShadows["useShadowDebugOverlay"] = "0";

        return definesWithShadows;
    }

    Shader::ShaderManager::DefineMap ShadowManager::getShadowsDisabledDefines()
    {
        Shader::ShaderManager::DefineMap definesWithShadows;
        definesWithShadows.insert(std::make_pair(std::string("shadows_enabled"), std::string("0")));
        definesWithShadows["shadow_texture_unit_list"] = "";

        definesWithShadows["shadowMapsOverlap"] = "0";

        definesWithShadows["useShadowDebugOverlay"] = "0";

        return definesWithShadows;
    }
    void ShadowManager::enableIndoorMode()
    {
        if (Settings::Manager::getBool("enable indoor shadows", "Shadows"))
            mShadowSettings->setCastsShadowTraversalMask(mIndoorShadowCastingMask);
        else
            mShadowTechnique->disableShadows();
    }
    void ShadowManager::enableOutdoorMode()
    {
        if (mEnableShadows)
            mShadowTechnique->enableShadows();
        mShadowSettings->setCastsShadowTraversalMask(mOutdoorShadowCastingMask);
    }
}
