
#include <osgEFT/NFX>

using namespace osgEFT;


std::string PrecompileShader::getShader(const std::string& shader_name)
{
    auto it = mShaderMap.find(shader_name);
    if (it == mShaderMap.end()) return "";

    std::string ret = it->second;

    //#include
    std::size_t found = ret.find("#include");
    int loop_count = 0;
    while (found != std::string::npos)
    {
        std::size_t v0 = ret.find("<", found);
        std::size_t v1 = ret.find(">", found);

        if (v0 != std::string::npos && v1 != std::string::npos)
        {
            std::string include_name = ret.substr(v0 + 1, v1 - v0 - 1);

            auto it2 = mShaderMap.find(include_name);
            if (it2 != mShaderMap.end())
            {
                ret.erase(found, v1 - found + 1);
                ret.insert(found, it2->second);
            }
            else
            {
                printf("not found shader :%s\n", include_name.c_str());
            }
        }
        else
        {
            printf("Invalid #include\n");
        }

        found = ret.find("#include", found);

        loop_count++;
        if (loop_count > 1000)
        {
            printf("find #inlcude over 1000 times. maybe samethiing error.\n");
            break;
        }
    }

    return ret;
}


NFX::NFX()
{
}

NFX::~NFX()
{
}

bool NFX::load(const std::string& filename)
{
    clean();

    dir = osgDB::getFilePath(filename);

    osgDB::XmlNode* root = osgDB::readXmlFile(filename);
    if (root == 0)
    {
        clean();
        return false;
    }

    osgDB::XmlNode* nfx;
    for (size_t i = 0; i < root->children.size(); i++)
    {
        osgDB::XmlNode* child = root->children[i];
        if (child->name == "nfx")
        {
            nfx = child;
            break;
        }
    }
    if (nfx == 0)
    {
        clean();
        return false;
    }


    for (size_t i = 0; i < nfx->children.size(); i++)
    {
        osgDB::XmlNode* child = nfx->children[i];

        if (child->name == "shader")
        {
            parserShader(child);
        }
        if (child->name == "texture")
        {
            parserTexutre(child);
        }
        if (child->name == "sufacematerial")
        {
            parserSufaceMaterial(child);
        }
        if (child->name == "slot")
        {
            parserSlot(child);
        }
        if (child->name == "compose")
        {
            parserCompose(child);
        }
    }

    return true;
}



//std::string NFX::save()
//{
//    return "";
//}

void NFX::clean()
{
    dir.clear();
    mShaderList.clear();
    mTexutreList.clear();
    mSufaceMaterialList.clear();
    mComposeList.clear();
}

osg::StateSet* NFX::getSufaceMaterial(const std::string& name)
{

    if (name.empty() && mSufaceMaterialList.size()>0)
    {
        return mSufaceMaterialList.rbegin()->second->mStateSet;
    }

    auto it = mSufaceMaterialList.find(name);
    if (it != mSufaceMaterialList.end())
    {
        return it->second->mStateSet;
    }

    return 0;
}

bool NFX::applyCompose(EffectManager* em
    , osg::Group* scene, osg::Camera* camera
    , const std::string& name)
{
    //cleanup
    em->cleanUp();

    //slot
    for (auto it = mSlotList.begin(); it != mSlotList.end(); it++)
    {
        //
        osg::Group* slot = new osg::Group();
        em->addSlot(it->first, slot);


        for (auto it2 = it->second->mFileList.begin(); it2 != it->second->mFileList.end(); it2++)
        {
            std::string filename = it2->first;
            osg::Vec3 pos = it2->second;

            osg::Node* node = osgDB::readNodeFile(dir + "/" + filename);
            if (node)
            {
                if (pos != osg::Vec3(0, 0, 0))
                {
                    osg::MatrixTransform* mt = new osg::MatrixTransform();
                    mt->setMatrix(osg::Matrix::translate(pos));

                    mt->addChild(node);
                    slot->addChild(mt);
                }
                else
                {
                    slot->addChild(node);
                }
            }
        }
    }

    //textrue and buffer

    //view pass
    int pid = -1;
    for (auto it = mComposeList.begin(); it != mComposeList.end(); it++)
    {
        for (size_t i = 0; i < it->second->mRenderViewList.size(); i++)
        {
            Compose::RenderView* rv = it->second->mRenderViewList[i];

            char name[64];
            sprintf(name, "pass%d", pid+1);

            // new pass
            if (rv->forward)
            {
                camera->setName(name);
                pid = em->addPassFromMainCamera(camera, scene
                    , rv->w, rv->h);
            }
            else
            {
                osg::Camera* pass_camera = new osg::Camera();
                pass_camera->setName(name);

                pid = em->addPass(pass_camera, rv->hud
                    , rv->render_order
                    , rv->w, rv->h, rv->x, rv->y);
            }

            // set state set
            osg::StateSet* ss = rv->mStateSet;
            em->setPassStateSet(pid,ss);

            //set input
            for (size_t ii = 0; ii < rv->mInputNames.size(); ii++)
            {
                if (rv->forward)
                {
                    //slot
                    em->setInput(pid, rv->mInputNames[ii]);
                }
                else
                {
                    //target
                    std::string target_name = rv->mInputNames[ii];
                    auto it = mTexutreList.find(target_name);
                    if (it != mTexutreList.end())
                    {
                        em->newTarget(target_name, it->second);
                    }

                    //buffer
                    em->setInput(pid, ii, target_name);
                }
            }

            //set output
            for (size_t ii = 0; ii < rv->mOutputs.size(); ii++)
            {
                Compose::OutputBuffer* ob = rv->mOutputs[ii];

                //target
                auto it = mTexutreList.find(ob->mName);
                if (it != mTexutreList.end())
                {
                    em->newTarget(ob->mName, it->second);
                }

                //
                em->setOutput(pid, ob->mBufferComponents, ob->mName, ob->mMultisampleSamples);
            }


        }//mRenderViewList
    }//mComposeList

    return true;
}


bool NFX::parserShader(osgDB::XmlNode* node)
{
    // name
    std::string name = node->properties["name"];

    // type
    NFX::ShaderType type = parserShaderType(node->properties["type"]);

    // text
    std::string text;
    for (size_t i = 0; i < node->children.size(); i++)
    {
        osgDB::XmlNode* child = node->children[i];

        if (child->name == "text")
        {
            if (child->children.size() > 0)
            {
                //CDATA
                text = child->children[0]->contents;
            }
            else
            {
                text = child->contents;
            }
        }
    }

    //
    ps.addShader(name, text);

    // new shader
    Shader* s = new Shader();
    s->name = name;
    s->type = type;
    s->text = text;
    mShaderList[name] = s;

    return true;
}

bool NFX::parserTexutre(osgDB::XmlNode* node)
{
    //name
    std::string name = node->properties["name"];

    // type
    NFX::TextureType type = parserTextureType(node->properties["type"]);

    // s t r format filename
    int s = 64;
    int t = 64;
    int r = 64;
    GLuint internal_format = 0;
    std::string filename;
    for (size_t i = 0; i < node->children.size(); i++)
    {
        osgDB::XmlNode* child = node->children[i];

        if (child->name == "image")
        {
            filename = child->properties["filename"];
        }
        if (child->name == "buffer")
        {
            s = atoi(child->properties["s"].c_str());
            t = atoi(child->properties["t"].c_str());
            r = atoi(child->properties["r"].c_str());
            internal_format = parserTextureFormat(child->properties["internal_format"]);
        }
    }

    //create image
    osg::ref_ptr<osg::Image> image;
    if (filename.size() > 0)
    {
        image = osgDB::readImageFile(dir + "/" + filename);
    }


    // default texture is 2d type
    if (type == NFX::TT_Null) type = TT_2D;


    //create osg::Texture instance
    osg::Texture* texture = 0;
    if (type == NFX::TT_1D)
    {
        osg::Texture1D* tex = new osg::Texture1D();
        tex->setResizeNonPowerOfTwoHint(false);
        texture = tex;
        if (image)
        {
            tex->setImage(image);
        }
        else
        {
            tex->setTextureWidth(s);
            tex->setFilter(osg::Texture2D::MIN_FILTER, osg::Texture2D::LINEAR);
            tex->setFilter(osg::Texture2D::MAG_FILTER, osg::Texture2D::LINEAR);
        }
    }
    if (type == NFX::TT_2D)
    {
        osg::Texture2D* tex = new osg::Texture2D();
        tex->setResizeNonPowerOfTwoHint(false);
        texture = tex;
        if (image)
        {
            tex->setImage(image);
        }
        else
        {
            tex->setTextureSize(s, t);
            tex->setFilter(osg::Texture2D::MIN_FILTER, osg::Texture2D::LINEAR);
            tex->setFilter(osg::Texture2D::MAG_FILTER, osg::Texture2D::LINEAR);
        }
    }
    if (type == NFX::TT_3D)
    {
        osg::Texture3D* tex = new osg::Texture3D();
        tex->setResizeNonPowerOfTwoHint(false);
        texture = tex;
        if (image)
        {
            tex->setImage(image);
        }
        else
        {
            tex->setTextureSize(s, t, r);
            tex->setFilter(osg::Texture2D::MIN_FILTER, osg::Texture2D::LINEAR);
            tex->setFilter(osg::Texture2D::MAG_FILTER, osg::Texture2D::LINEAR);
        }
    }


    //
    texture->setName(name);
    if (internal_format > 0) texture->setInternalFormat(internal_format);
    //texture->setSourceFormat(internal_format);
    texture->dirtyTextureObject();
    mTexutreList[name] = texture;

    return true;
}

bool NFX::parserSufaceMaterial(osgDB::XmlNode* node)
{
    osg::StateSet* ss = new osg::StateSet();

    // name
    std::string name = node->properties["name"];
    ss->setName(name);

    // text
    for (size_t i = 0; i < node->children.size(); i++)
    {
        osgDB::XmlNode* child = node->children[i];

        if (child->name == "program")
        {
            osg::Program* prop = parserProgram(child);
            ss->setAttributeAndModes(prop);

            // parameter
            //auto vs_shader = mShaderList.find();

        }//program

        if (child->name == "uniform")
        {
            osg::Uniform* u = parserUniform(child);
            if (u) ss->addUniform(u);
        }//uniform

        if (child->name == "sampler")
        {
            std::string name = child->properties["name"];
            std::string texture = child->properties["texture"];
            int unit = atoi(child->properties["unit"].c_str());

            auto it = mTexutreList.find(texture);
            if (it != mTexutreList.end())
            {
                ss->setTextureAttributeAndModes(unit, it->second);
                ss->addUniform(new osg::Uniform(name.c_str(), unit));
            }
        }//sampler

    }//for

    //
    SufaceMaterial* sm = new SufaceMaterial();
    sm->name = name;
    sm->mStateSet = ss;
    mSufaceMaterialList[name] = sm;

    return true;
}

bool NFX::parserSlot(osgDB::XmlNode* node)
{
    // name
    std::string name = node->properties["name"];

    //
    Slot* slot = new Slot();
    slot->name = name;
    mSlotList[name] = slot;

    // text
    std::string text;
    for (size_t i = 0; i < node->children.size(); i++)
    {
        osgDB::XmlNode* child = node->children[i];

        if (child->name == "model")
        {
            std::string filename = child->properties["filename"];
            float x = atof(child->properties["x"].c_str());
            float y = atof(child->properties["y"].c_str());
            float z = atof(child->properties["z"].c_str());

            slot->mFileList.insert(
                std::pair<std::string, osg::Vec3>(filename,osg::Vec3(x,y,z)));
        }
    }

    return true;
}

bool NFX::parserCompose(osgDB::XmlNode* node)
{
    std::string name = node->properties["name"];

    Compose* compose = new Compose();
    mComposeList[name] = compose;

    //view
    for (size_t i = 0; i < node->children.size(); i++)
    {
        osgDB::XmlNode* child = node->children[i];

        std::string hud = child->properties["hud"];
        std::string forward = child->properties["forward"];
        
        std::string xa = child->properties["xa"];
        std::string xr = child->properties["xr"];
        std::string ya = child->properties["ya"];
        std::string yr = child->properties["yr"];
        std::string wa = child->properties["wa"];
        std::string wr = child->properties["wr"];
        std::string ha = child->properties["ha"];
        std::string hr = child->properties["hr"];

        Compose::RenderView* rv = new Compose::RenderView();
        compose->mRenderViewList.push_back(rv);

        rv->hud = parserBool(hud);
        rv->forward = parserBool(forward);

        if (xa.size() > 0 && xr.size() > 0) rv->x = osgEFT::Coord(atof(xr.c_str()), atoi(xa.c_str()));
        if (ya.size() > 0 && yr.size() > 0) rv->y = osgEFT::Coord(atof(yr.c_str()), atoi(ya.c_str()));
        if (wa.size() > 0 && wr.size() > 0) rv->w = osgEFT::Coord(atof(wr.c_str()), atoi(wa.c_str()));
        if (ha.size() > 0 && hr.size() > 0) rv->h = osgEFT::Coord(atof(hr.c_str()), atoi(ha.c_str()));


        for (size_t j = 0; j < child->children.size(); j++)
        {
            osgDB::XmlNode* view_child = child->children[j];


            if (view_child->name == "uniform"
                || view_child->name == "program")
            {
               if(!rv->mStateSet.valid()) rv->mStateSet = new osg::StateSet();
            }

            if (view_child->name == "input")
            {
                std::string target_name = view_child->properties["target_name"];
                rv->mInputNames.push_back(target_name);
            }
            if (view_child->name == "output")
            {
                std::string target_name = view_child->properties["target_name"];
                osg::Camera::BufferComponent buffercomponent = parserBufferComponent(view_child->properties["buffercomponent"]);
                int multisamplesamples = atoi(view_child->properties["multisamplesamples"].c_str());

                Compose::OutputBuffer* ob = new Compose::OutputBuffer();
                ob->mName = target_name;
                ob->mBufferComponents = buffercomponent;
                ob->mMultisampleSamples = multisamplesamples;

                rv->mOutputs.push_back(ob);
            }
            if (view_child->name == "updater")
            {
            }
            if (view_child->name == "uniform")
            {
                osg::Uniform* u = parserUniform(view_child);
                if (u) rv->mStateSet->addUniform(u);
            }
            if (view_child->name == "program")
            {
                osg::Program* prop = parserProgram(view_child);
                rv->mStateSet->setAttributeAndModes(prop);
            }



        }
    }

    return true;
}


NFX::TextureType NFX::parserTextureType(const std::string& str)
{
    if (str == "1d") return NFX::TT_1D;
    if (str == "2d") return NFX::TT_2D;
    if (str == "3d") return NFX::TT_3D;
    return NFX::TT_Null;
}
GLuint NFX::parserTextureFormat(const std::string& str)
{
    if (str == "LUMINANCE32") return GL_LUMINANCE32F_ARB;

    if (str == "RGB") return GL_RGB;
    if (str == "RGBA") return GL_RGBA;
    if (str == "RGBA16") return GL_RGBA16F_ARB;
    if (str == "RGBA32") return GL_RGBA32F_ARB;

    if (str == "DEPTH") return GL_DEPTH_COMPONENT;
    if (str == "DEPTH16") return GL_DEPTH_COMPONENT16;
    if (str == "DEPTH24") return GL_DEPTH_COMPONENT24;

    return 0;
}
NFX::ShaderType NFX::parserShaderType(const std::string& str)
{
    if (str == "vs") return NFX::ST_VS;
    if (str == "fs") return NFX::ST_FS;
    if (str == "gs") return NFX::ST_GS;
    return NFX::ST_Null;
}
osg::Camera::BufferComponent NFX::parserBufferComponent(const std::string& str)
{
    if (str == "DEPTH_BUFFER") return osg::Camera::BufferComponent::DEPTH_BUFFER;
    if (str == "STENCIL_BUFFER") return osg::Camera::BufferComponent::STENCIL_BUFFER;
    if (str == "PACKED_DEPTH_STENCIL_BUFFER") return osg::Camera::BufferComponent::PACKED_DEPTH_STENCIL_BUFFER;
    if (str == "COLOR_BUFFER") return osg::Camera::BufferComponent::COLOR_BUFFER;

    if (str == "COLOR_BUFFER0") return osg::Camera::BufferComponent::COLOR_BUFFER0;
    if (str == "COLOR_BUFFER1") return osg::Camera::BufferComponent::COLOR_BUFFER1;
    if (str == "COLOR_BUFFER2") return osg::Camera::BufferComponent::COLOR_BUFFER2;
    if (str == "COLOR_BUFFER3") return osg::Camera::BufferComponent::COLOR_BUFFER3;
    if (str == "COLOR_BUFFER4") return osg::Camera::BufferComponent::COLOR_BUFFER4;
    if (str == "COLOR_BUFFER5") return osg::Camera::BufferComponent::COLOR_BUFFER5;
    if (str == "COLOR_BUFFER6") return osg::Camera::BufferComponent::COLOR_BUFFER6;
    if (str == "COLOR_BUFFER7") return osg::Camera::BufferComponent::COLOR_BUFFER7;
    if (str == "COLOR_BUFFER8") return osg::Camera::BufferComponent::COLOR_BUFFER8;
    if (str == "COLOR_BUFFER9") return osg::Camera::BufferComponent::COLOR_BUFFER9;
    if (str == "COLOR_BUFFER10") return osg::Camera::BufferComponent::COLOR_BUFFER10;
    if (str == "COLOR_BUFFER11") return osg::Camera::BufferComponent::COLOR_BUFFER11;
    if (str == "COLOR_BUFFER12") return osg::Camera::BufferComponent::COLOR_BUFFER12;
    if (str == "COLOR_BUFFER13") return osg::Camera::BufferComponent::COLOR_BUFFER13;
    if (str == "COLOR_BUFFER14") return osg::Camera::BufferComponent::COLOR_BUFFER14;
    if (str == "COLOR_BUFFER15") return osg::Camera::BufferComponent::COLOR_BUFFER15;

    return osg::Camera::BufferComponent::COLOR_BUFFER;
}

osg::Program* NFX::parserProgram(osgDB::XmlNode* node)
{
    std::string vs = node->properties["vs"];
    std::string fs = node->properties["fs"];
    std::string gs = node->properties["gs"];

    osg::Program* prop = new osg::Program();
    if (vs.size() > 0)
    {
        std::string vs_string = ps.getShader(vs);
        prop->addShader(new osg::Shader(osg::Shader::Type::VERTEX, vs_string));
    }
    if (fs.size() > 0)
    {
        std::string fs_string = ps.getShader(fs);
        prop->addShader(new osg::Shader(osg::Shader::Type::FRAGMENT, fs_string));
    }
    if (gs.size() > 0)
    {
        std::string gs_string = ps.getShader(gs);
        prop->addShader(new osg::Shader(osg::Shader::Type::GEOMETRY, gs_string));
    }
    
    return prop;
}

osg::Uniform* NFX::parserUniform(osgDB::XmlNode* node)
{
    std::string uname = node->properties["name"];
    std::string utype = node->properties["type"];

    std::string text = node->contents;
    std::vector< std::string > sl = split(text, " ");

    osg::Uniform* uniform = 0;

    //bool
    if (utype == "bool")
    {
        bool f0 = parserBool(sl[0].c_str());
        uniform = new osg::Uniform(uname.c_str(), f0);
    }
    if (utype == "bool2")
    {
        bool f0 = parserBool(sl[0].c_str());
        bool f1 = parserBool(sl[1].c_str());
        uniform = new osg::Uniform(uname.c_str(), f0, f1);
    }
    if (utype == "bool3")
    {
        bool f0 = parserBool(sl[0].c_str());
        bool f1 = parserBool(sl[1].c_str());
        bool f2 = parserBool(sl[2].c_str());
        uniform = new osg::Uniform(uname.c_str(), f0, f1, f2);
    }
    if (utype == "bool4")
    {
        bool f0 = parserBool(sl[0].c_str());
        bool f1 = parserBool(sl[1].c_str());
        bool f2 = parserBool(sl[2].c_str());
        bool f3 = parserBool(sl[3].c_str());
        uniform = new osg::Uniform(uname.c_str(), f0, f1, f2, f3);
    }


    //int
    if (utype == "int")
    {
        int f0 = atoi(sl[0].c_str());
        uniform = new osg::Uniform(uname.c_str(), f0);
    }
    if (utype == "int2")
    {
        int f0 = atoi(sl[0].c_str());
        int f1 = atoi(sl[1].c_str());
        uniform = new osg::Uniform(uname.c_str(), f0, f1);
    }
    if (utype == "int3")
    {
        int f0 = atoi(sl[0].c_str());
        int f1 = atoi(sl[1].c_str());
        int f2 = atoi(sl[2].c_str());
        uniform = new osg::Uniform(uname.c_str(), f0, f1, f2);
    }
    if (utype == "int4")
    {
        int f0 = atoi(sl[0].c_str());
        int f1 = atoi(sl[1].c_str());
        int f2 = atoi(sl[2].c_str());
        int f3 = atoi(sl[3].c_str());
        uniform = new osg::Uniform(uname.c_str(), f0, f1, f2, f3);
    }

    //uint
    if (utype == "uint")
    {
        unsigned int f0 = atoi(sl[0].c_str());
        uniform = new osg::Uniform(uname.c_str(), f0);
    }
    if (utype == "uint2")
    {
        unsigned int f0 = atoi(sl[0].c_str());
        unsigned int f1 = atoi(sl[1].c_str());
        uniform = new osg::Uniform(uname.c_str(), f0, f1);
    }
    if (utype == "uint3")
    {
        unsigned int f0 = atoi(sl[0].c_str());
        unsigned int f1 = atoi(sl[1].c_str());
        unsigned int f2 = atoi(sl[2].c_str());
        uniform = new osg::Uniform(uname.c_str(), f0, f1, f2);
    }
    if (utype == "uint4")
    {
        unsigned int f0 = atoi(sl[0].c_str());
        unsigned int f1 = atoi(sl[1].c_str());
        unsigned int f2 = atoi(sl[2].c_str());
        unsigned int f3 = atoi(sl[3].c_str());
        uniform = new osg::Uniform(uname.c_str(), f0, f1, f2, f3);
    }

    //float
    if (utype == "float")
    {
        float f0 = atof(sl[0].c_str());
        uniform = new osg::Uniform(uname.c_str(), f0);
    }
    if (utype == "float2")
    {
        float f0 = atof(sl[0].c_str());
        float f1 = atof(sl[1].c_str());
        uniform = new osg::Uniform(uname.c_str(), osg::Vec2f(f0, f1));
    }
    if (utype == "float3")
    {
        float f0 = atof(sl[0].c_str());
        float f1 = atof(sl[1].c_str());
        float f2 = atof(sl[2].c_str());
        uniform = new osg::Uniform(uname.c_str(), osg::Vec3f(f0, f1, f2));
    }
    if (utype == "float4")
    {
        float f0 = atof(sl[0].c_str());
        float f1 = atof(sl[1].c_str());
        float f2 = atof(sl[2].c_str());
        float f3 = atof(sl[3].c_str());
        uniform = new osg::Uniform(uname.c_str(), osg::Vec4f(f0, f1, f2, f3));
    }

    //double
    if (utype == "double")
    {
        double f0 = atof(sl[0].c_str());
        uniform = new osg::Uniform(uname.c_str(), f0);
    }
    if (utype == "double2")
    {
        double f0 = atof(sl[0].c_str());
        double f1 = atof(sl[1].c_str());
        uniform = new osg::Uniform(uname.c_str(), osg::Vec2d(f0, f1));
    }
    if (utype == "double3")
    {
        double f0 = atof(sl[0].c_str());
        double f1 = atof(sl[1].c_str());
        double f2 = atof(sl[2].c_str());
        uniform = new osg::Uniform(uname.c_str(), osg::Vec3d(f0, f1, f2));
    }
    if (utype == "double4")
    {
        double f0 = atof(sl[0].c_str());
        double f1 = atof(sl[1].c_str());
        double f2 = atof(sl[2].c_str());
        double f3 = atof(sl[3].c_str());
        uniform = new osg::Uniform(uname.c_str(), osg::Vec4d(f0, f1, f2, f3));
    }

    //
    if (utype == "matrixf")
    {
    }
    if (utype == "matrixd")
    {
    }


    return uniform;
}


bool NFX::parserBool(const std::string& str)
{
    if (str == "t"|| str == "T"
        || str == "true" || str == "True"
        || str == "TRUE"
        )
    {
        return true;
    }
    return false;
}
