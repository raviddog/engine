#include "engine.hpp"

#include "debug.hpp"

#include <chrono>
#include <thread>
#include <string>
#include <fstream>
#include <unordered_map>

#include "libs/imgui/imgui.h"
#include "libs/imgui/imgui_impl_glfw.h"
#include "libs/imgui/imgui_impl_opengl3.h"

#include "libs/ini.h"

#ifdef _MSC_VER
#define WIN32_LEAN_AND_MEAN
#include "windows.h"
#include "timeapi.h"
#endif

#ifdef __GNUG__
#include "pthread.h"
#endif

#include "OBJ_Loader.h"

/*
 *
 *      Sections in file:
 * 
 *      [TEXT]
 *      [3DMODEL]
 *      [SPRITE]
 *      [DRAW]
 *      [INIT]
 *      [FLIP]
 *      [IMGUI]
 *      [ASSETSYS]
 * 
 * 
 * 
 * */

namespace engine {
    Camera3D *Camera3D::bound = nullptr;

    bool loadFromZip = false;
    
    assetsys_t *assets = nullptr;

    int scrWidth, scrHeight, drawWidth, drawHeight;
    bool maximised = false;
    int viewport[4], scrMode;
    float scalex, scaley;
    
    int aspect_w, aspect_h, winflags;
        
    double deltatime, last_frame;
    
    uint32_t controls[controlSize];

    //  framerate stuff
    static bool _vsync, _fixeddrawsize = true;
    uint32_t fps;
    double ticks, frameTimeTicks, avgTicksTotal;
    std::chrono::high_resolution_clock::time_point cur_time, next_time;
    //  v this sucks v
    #define _ENGINE_FPS_CAP 60
    #define _ENGINE_NOVSYNC_DELAY_MICROSECONDS 1000000 / _ENGINE_FPS_CAP
    
    //  imgui stuff
    //  store type, pointer to value
    struct imgui_t{
        std::string text;
        bool edit;
        int type;
        void *value;
    };
    std::vector<std::pair<std::string, std::vector<imgui_t>*>> *imgui_windows = nullptr;
    


    gl::Shader *shaderSpriteSheet, *shaderSpriteSheetInvert, *shaderUI, *pshader, *shader3d;

    static Drawmode currentDrawmode;

    void windowMaximiseCallback(GLFWwindow*, int);
    void windowResizeCallback(GLFWwindow*, int, int);
    void errorCallback(int, const char*);

    int gcd(int a, int b) {
        return b ? gcd(b, a % b) : a;
    }

    void aspectRatio(int *x, int *y) {
        int d = gcd(*x, *y);
        *x = *x / d;
        *y = *y / d;
    }

    void aspectRatio(float *x, float *y) {
        float d = gcd(*x, *y);
        *x = *x / d;
        *y = *y / d;
    }

    //  [TEXT]
    BitmapFont::BitmapFont(std::string path) {
        //  NOTE assume ascii only for now
        //  ascii is 16x8
        s = new SpriteSheet(path, 128);

        gl::Texture *t = s->tex;
        charinfo.w = t->srcWidth / 16;
        charinfo.h = t->srcHeight / 8;

        for(size_t i = 0; i < 128; i++) {
            int x = i % 16 * charinfo.w;
            int y = i / 16 * charinfo.h;
            s->setSprite(i, x, y, charinfo.w, charinfo.h);
        }
    }

    BitmapFont::~BitmapFont() {
        delete s;
    }

    void BitmapFont::Dimensions(std::string text, int *w, int *h) {
        int lines = 0;
        int maxwidth = 0;
        int cwidth = 0;
        for(size_t i = 0; i < text.length(); i++) {
            if(text.at(i) == '\n') {
                lines++;
                cwidth = 0;
            } else {
                cwidth++;
                if(cwidth > maxwidth) {
                    maxwidth = cwidth;
                }
            }
        }

        *w = maxwidth * charinfo.w;
        *h = lines * charinfo.h;
    }

    void BitmapFont::Dimensions(std::string text, int *w, int *h, int *s) {
        int lines = 0;
        int maxwidth = 0;
        int cwidth = 0;
        for(size_t i = 0; i < text.length(); i++) {
            if(text.at(i) == '\n') {
                lines++;
                cwidth = 0;
            } else {
                cwidth++;
                if(cwidth > maxwidth) {
                    maxwidth = cwidth;
                }
            }
        }

        *w = maxwidth * *s;
        *h = lines * (*s / charinfo.w) * charinfo.h;
    }

    void BitmapFont::Write(std::string text, float x, float y) {
        int line = 0;
        int c = 0;

        for(size_t i = 0; i < text.length(); i++) {
            if(text.at(i) == '\n') {
                line++;
                c = 0;
            } else {
                s->drawSprite((int)text.at(i), x + c * charinfo.w, y + line * charinfo.h);
                c++;
            }
        }
    }

    void BitmapFont::WriteCentered(std::string text, float x, float y) {
        int w, h;
        Dimensions(text, &w, &h);

        x -= w / 2;
        y -= h / 2;

        int line = 0;
        int c = 0;

        for(size_t i = 0; i < text.length(); i++) {
            if(text.at(i) == '\n') {
                line++;
                c = 0;
            } else {
                s->drawSprite((int)text.at(i), x + c * charinfo.w, y + line * charinfo.h);
                c++;
            }
        }
    }

    void BitmapFont::Write(std::string text, float x, float y, int w) {
        int line = 0;
        int c = 0;

        for(size_t i = 0; i < text.length(); i++) {
            if(text.at(i) == '\n') {
                line++;
                c = 0;
            } else {
                s->drawSprite((int)text.at(i), x + c * w, y + line * charinfo.h * (w / charinfo.w), 0.f, w, charinfo.h * (w / charinfo.w));
                c++;
            }
        }
    }

    void BitmapFont::WriteCentered(std::string text, float x, float y, int w) {
        int wx, h;
        Dimensions(text, &wx, &h, &w);

        x -= wx / 2;
        y -= h / 2;

        int line = 0;
        int c = 0;

        for(size_t i = 0; i < text.length(); i++) {
            if(text.at(i) == '\n') {
                line++;
                c = 0;
            } else {
                s->drawSprite((int)text.at(i), x + c * w, y + line * charinfo.h * (w / charinfo.w), 0.f, w, charinfo.h * (w / charinfo.w));
                c++;
            }
        }
    }

    void BitmapFont::buffer() {
        s->buffer();
    }

    void BitmapFont::draw() {
        s->draw();
    }

//  [3DMODEL]
    class ManagedModel {
        public:
            int count = 1;
            Model *model;

            ManagedModel(std::string&);
            ~ManagedModel();
    };

    ManagedModel::ManagedModel(std::string &path) {
        model = new Model(path.c_str());
        count = 1;
    }

    ManagedModel::~ManagedModel() {
        delete model;
    }

    std::unordered_map<std::string, ManagedModel*> *loadedModels;

    Model* LoadModel(std::string &path) {
        if(loadedModels->count(path)) {
            ManagedModel *tm = loadedModels->at(path);
            tm->count++;
            return tm->model;
        } else {
            ManagedModel *tm = new ManagedModel(path);
            loadedModels->insert(std::make_pair(path, tm));
            return tm->model;
        }
    }

    void UnloadModel(std::string &path) {
        if(loadedModels->count(path)) {
            ManagedModel *tm = loadedModels->at(path);
            tm->count--;
            if(tm->count <= 0) {
                loadedModels->erase(path);
                delete tm;
            }
        } 
    }

    void UnloadModel(Model* md) {
        UnloadModel(md->path);
    }

    float quadVertices[] = { // vertex attributes for a quad that fills the entire viewport in Normalized Device Coordinates.
        // positions   // texCoords
        -1.0f,  1.0f,  0.0f, 1.0f,
        -1.0f, -1.0f,  0.0f, 0.0f,
         1.0f, -1.0f,  1.0f, 0.0f,

        -1.0f,  1.0f,  0.0f, 1.0f,
         1.0f, -1.0f,  1.0f, 0.0f,
         1.0f,  1.0f,  1.0f, 1.0f
    };

    Model::Model(const char *rawpath) {
        path = std::string(rawpath);

        materials = new std::vector<Material_t>();
        meshes = new std::vector<Mesh_t>();
        //  texture map
        textures = new std::unordered_map<std::string, gl::Texture*>();

        std::string directory = path.substr(0, path.find_last_of('/'));
        directory += '/';

        objl::Loader loader;
        if(loader.LoadFile(path.c_str())) {

            //  loop through and pull loaded materials
            //  also load textures as we go
            for(auto loadedMat = loader.LoadedMaterials.begin(); loadedMat != loader.LoadedMaterials.end(); loadedMat++) {
                //  load material
                Material_t material;
                material.name = loadedMat->name;
                material.colourAmbient = glm::vec3(loadedMat->Ka.X, loadedMat->Ka.Y, loadedMat->Ka.Z);
                material.colourDiffuse = glm::vec3(loadedMat->Kd.X, loadedMat->Kd.Y, loadedMat->Kd.Z);
                material.colourSpecular = glm::vec3(loadedMat->Ks.X, loadedMat->Ks.Y, loadedMat->Ks.Z);
                material.specularExponent = loadedMat->Ns;
                material.opticalDensity = loadedMat->Ni;
                material.dissolve = loadedMat->d;
                material.illumination = loadedMat->illum;
                material.texname_ambientMap = loadedMat->map_Ka;
                material.texname_diffuseMap = loadedMat->map_Kd;
                material.texname_specularMap = loadedMat->map_Ks;
                material.texname_alpha = loadedMat->map_d;
                material.texname_bump = loadedMat->map_bump;

                //  check if textures are loaded and if not, load them
                if(material.texname_ambientMap != "" && textures->count(material.texname_ambientMap) == 0) {
                    gl::Texture *tex = new gl::Texture();
                    tex->bind();
                    tex->load(directory + material.texname_ambientMap);
                    textures->insert(std::make_pair(material.texname_ambientMap, tex));
                }
                if(material.texname_diffuseMap != "" && textures->count(material.texname_diffuseMap) == 0) {
                    gl::Texture *tex = new gl::Texture();
                    tex->bind();
                    tex->load(directory + material.texname_diffuseMap);
                    textures->insert(std::make_pair(material.texname_diffuseMap, tex));
                }
                if(material.texname_specularMap != "" && textures->count(material.texname_specularMap) == 0) {
                    gl::Texture *tex = new gl::Texture();
                    tex->bind();
                    tex->load(directory + material.texname_specularMap);
                    textures->insert(std::make_pair(material.texname_specularMap, tex));
                }
                if(material.texname_alpha != "" && textures->count(material.texname_alpha) == 0) {
                    gl::Texture *tex = new gl::Texture();
                    tex->bind();
                    tex->load(directory + material.texname_alpha);
                    textures->insert(std::make_pair(material.texname_alpha, tex));
                }
                if(material.texname_bump != "" && textures->count(material.texname_bump) == 0) {
                    gl::Texture *tex = new gl::Texture();
                    tex->bind();
                    tex->load(directory + material.texname_bump);
                    textures->insert(std::make_pair(material.texname_bump, tex));
                }
                
                materials->push_back(material);
            }

            //  loop through meshes and do stuff?
            // for(auto loadedMesh = loader.LoadedMeshes.begin(); loadedMesh != loader.LoadedMeshes.end(); loadedMesh++) {
            for(uint32_t i = 0u; i < loader.LoadedMeshes.size(); i++) {
                auto loadedMesh = &loader.LoadedMeshes[i];

                Mesh_t mesh;
                mesh.name = loadedMesh->MeshName;
                mesh.vao = std::shared_ptr<gl::VAO>(new gl::VAO);
                mesh.vao->bind();
                mesh.vbo = std::shared_ptr<gl::VBO>(new gl::VBO);
                mesh.vbo->bind();
                mesh.vbo->createVertexAttribPointer(3, GL_FLOAT, 8 * sizeof(float), (void*)0);
                mesh.vbo->createVertexAttribPointer(3, GL_FLOAT, 8 * sizeof(float), (void*)(3 * sizeof(float)));
                mesh.vbo->createVertexAttribPointer(2, GL_FLOAT, 8 * sizeof(float), (void*)(6 * sizeof(float)));

                //  build vector of floats to buffer
                std::vector<float> vertices;
                std::vector<uint32_t> indices;
                // for(auto vertex = loadedMesh->Vertices.begin(); vertex != loadedMesh->Vertices.end(); vertex++) {
                for(uint32_t j = 0u; j < loadedMesh->Vertices.size(); j++) {
                    auto vertex = &loadedMesh->Vertices[j];
                    vertices.push_back(vertex->Position.X);
                    vertices.push_back(vertex->Position.Y);
                    vertices.push_back(vertex->Position.Z);
                    vertices.push_back(vertex->Normal.X);
                    vertices.push_back(vertex->Normal.Y);
                    vertices.push_back(vertex->Normal.Z);
                    vertices.push_back(vertex->TextureCoordinate.X);
                    vertices.push_back(vertex->TextureCoordinate.Y);

                    indices.push_back(j);
                }
                
                mesh.vbo->bufferVerts(  sizeof(float) * vertices.size(),
                                        vertices.data(),
                                        sizeof(uint32_t) * indices.size(),
                                        indices.data());

                mesh.vao->unbind();
                //  save number of indices
                mesh.indices = indices.size();
                //  find material name in vector
                size_t matCount = 0;
                for(; matCount < materials->size() && materials->at(matCount).name != loadedMesh->MeshMaterial.name; matCount++);
                if(matCount < materials->size()) {
                    //  material found
                    mesh.material = &materials->at(matCount);
                } else {
                    mesh.material = nullptr;
                }
                
                meshes->push_back(mesh);
            }
            gl::VAO::unbind();


        } else {
            //  load failed, error something
        }
    }

    void Model::draw() {
        for(auto i = meshes->begin(); i != meshes->end(); i++) {
            i->vao->bind();
            if(i->material) {
                if(textures->at(i->material->texname_diffuseMap)) {
                    textures->at(i->material->texname_diffuseMap)->bind(0);
                }
                if(textures->at(i->material->texname_specularMap)) {
                    textures->at(i->material->texname_specularMap)->bind(1);
                }
                // if(textures->at(i->material->texname_ambientMap)) {
                //     textures->at(i->material->texname_ambientMap)->bind(2);
                // }
                gl::Texture::activateUnit(0);
            }
            glDrawElements(GL_TRIANGLES, i->indices, GL_UNSIGNED_INT, (void*)0);
        }
    }

    Model::~Model() {
        gl::VAO::unbind();
        delete meshes;
        delete materials;
        emptyMap(textures);
        delete textures;
    }

    //  [SPRITE]

    SpriteSheet::SpriteSheet(std::string path, int numSprites) {
        load(path, numSprites);
    }

    SpriteSheet::SpriteSheet(std::string path, int numSprites, size_t maxDraws) {
        load(path, numSprites, maxDraws);
    }

    SpriteSheet::~SpriteSheet() {
        delete tex;
        delete vbo;
        delete vao;
        delete verts;
        delete indices;
        delete sprites;
    }

    void SpriteSheet::load(std::string path, int numSprites) {
        realloc = true;

        vao = new gl::VAO();
        vbo = new gl::VBO();
        
        vao->bind();
        vbo->bind();
        vbo->createVertexAttribPointer(2, GL_FLOAT, 7*sizeof(float), (void*)0);
        vbo->createVertexAttribPointer(2, GL_FLOAT, 7*sizeof(float), (void*)(2*sizeof(float)));
        vbo->createVertexAttribPointer(2, GL_FLOAT, 7*sizeof(float), (void*)(4*sizeof(float)));
        vbo->createVertexAttribPointer(1, GL_FLOAT, 7*sizeof(float), (void*)(6*sizeof(float)));
        vbo->unbind();

        this->numSprites = numSprites;
        sprites = new Sprite[numSprites];
        tex = new gl::Texture();
        tex->bind();
        tex->load(path);
        tex->unbind();

        vao->unbind();

        verts = new std::vector<float>();
        indices = new std::vector<uint32_t>();

        indices_stored_size = 0;
    }

    void SpriteSheet::load(std::string path, int numSprites, size_t maxDraws) {
        load(path, numSprites);
        vbo->bind();
        vbo->createBuffer(sizeof(float) * 28 * maxDraws, sizeof(uint32_t) * 6 * maxDraws);
        vbo->unbind();
        realloc = false;
        verts->reserve(maxDraws * 28);
        indices->reserve(maxDraws * 6);
    }

    void SpriteSheet::setSprite(int num, int x, int y, int width, int height)
    {
        if(num < numSprites && num >= 0) {
            //  normalised texture coordinates
            sprites[num].x = (1.0f / (float)tex->srcWidth) * (float)x;
            sprites[num].y = (1.0f / (float)tex->srcHeight) * (float)y;
            sprites[num].z = (1.0f / (float)tex->srcWidth) * (float)(x + width);
            sprites[num].w = (1.0f / (float)tex->srcHeight) * (float)(y + height);
            //  size in pixels, is normalized to screen size in shader
            sprites[num].width = (float)width;
            sprites[num].height = (float)height;
        }
        #ifdef _MSG_DEBUG_ENABLED_SPRITE
        log_debug("sprite #%d: %f %f %f %f\n", num, sprites[num].x, sprites[num].y, sprites[num].z, sprites[num].w);
        #endif
    }

    void SpriteSheet::drawSprite(int num, float x, float y) {
        drawSprite(num, x, y, 0.0f);
    }

    void SpriteSheet::drawSprite(int num, float x, float y, float angle) {
        if(num > -1 && num < numSprites) {
            float v[] {
                x + sprites[num].width / 2.f, y + sprites[num].height / 2.f, sprites[num].x, sprites[num].w, -sprites[num].width, -sprites[num].height, angle,
                x + sprites[num].width / 2.f, y + sprites[num].height / 2.f, sprites[num].x, sprites[num].y, -sprites[num].width, sprites[num].height, angle,
                x + sprites[num].width / 2.f, y + sprites[num].height / 2.f, sprites[num].z, sprites[num].y, sprites[num].width, sprites[num].height, angle,
                x + sprites[num].width / 2.f, y + sprites[num].height / 2.f, sprites[num].z, sprites[num].w, sprites[num].width, -sprites[num].height, angle

            };

            uint32_t size = verts->size() / 7;

            uint32_t ind[] = {
                size, size + 1, size + 2,
                size, size + 3, size + 2
            };

            verts->insert(verts->end(), std::begin(v), std::end(v));
            indices->insert(indices->end(), std::begin(ind), std::end(ind));
        }
    }

    void SpriteSheet::drawSprite(int num, float x, float y, float angle, float width, float height) {
        if(num > -1 && num < numSprites) {
            float v[] {
                x + width / 2.f, y + height / 2.f, sprites[num].x, sprites[num].w, -width, -height, angle,
                x + width / 2.f, y + height / 2.f, sprites[num].x, sprites[num].y, -width, height, angle,
                x + width / 2.f, y + height / 2.f, sprites[num].z, sprites[num].y, width, height, angle,
                x + width / 2.f, y + height / 2.f, sprites[num].z, sprites[num].w, width, -height, angle

            };

            uint32_t size = verts->size() / 7;

            uint32_t ind[] = {
                size, size + 1, size + 2,
                size, size + 3, size + 2
            };

            verts->insert(verts->end(), std::begin(v), std::end(v));
            indices->insert(indices->end(), std::begin(ind), std::end(ind));
        }
    }

    void SpriteSheet::drawSpriteCentered(int num, float x, float y) {
        drawSpriteCentered(num, x, y, 0.0f);
    }

    void SpriteSheet::drawSpriteCentered(int num, float x, float y, float angle) {
        if(num > -1 && num < numSprites) {
            float v[] {
                x, y, sprites[num].x, sprites[num].w, -sprites[num].width, -sprites[num].height, angle,
                x, y, sprites[num].x, sprites[num].y, -sprites[num].width, sprites[num].height, angle,
                x, y, sprites[num].z, sprites[num].y, sprites[num].width, sprites[num].height, angle,
                x, y, sprites[num].z, sprites[num].w, sprites[num].width, -sprites[num].height, angle

            };

            uint32_t size = verts->size() / 7;

            uint32_t ind[] = {
                size, size + 1, size + 2,
                size, size + 3, size + 2
            };

            verts->insert(verts->end(), std::begin(v), std::end(v));
            indices->insert(indices->end(), std::begin(ind), std::end(ind));
        }
    }

    void SpriteSheet::drawSpriteCentered(int num, float x, float y, float angle, float width, float height) {
        if(num > -1 && num < numSprites) {
            float v[] {
                x, y, sprites[num].x, sprites[num].w, -width, -height, angle,
                x, y, sprites[num].x, sprites[num].y, -width, height, angle,
                x, y, sprites[num].z, sprites[num].y, width, height, angle,
                x, y, sprites[num].z, sprites[num].w, width, -height, angle

            };

            uint32_t size = verts->size() / 7;

            uint32_t ind[] = {
                size, size + 1, size + 2,
                size, size + 3, size + 2
            };

            verts->insert(verts->end(), std::begin(v), std::end(v));
            indices->insert(indices->end(), std::begin(ind), std::end(ind));
        }
    }

    void SpriteSheet::buffer() {
        vao->bind();
        vbo->bind();
        if(realloc) {
            vbo->bufferVerts(sizeof(float) * verts->size(), verts->data(), sizeof(uint32_t) * indices->size(), indices->data());
        } else {
            vbo->bufferSubVerts(sizeof(float) * verts->size(), verts->data(), sizeof(uint32_t) * indices->size(), indices->data());
        }
        indices_stored_size = indices->size();
        vao->unbind();
        verts->clear();
        indices->clear();
    }

    void SpriteSheet::draw() {
        vao->bind();
        tex->bind();
        glDrawElements(GL_TRIANGLES, indices_stored_size, GL_UNSIGNED_INT, (void*)0);
        vao->unbind();
    }

    void SpriteSheet::useShaderInvert() {
        SetDrawmode(DrawmodeSpriteInvert);
    }

    void SpriteSheet::useShaderNormal() {
        SetDrawmode(DrawmodeSprite);
    }

    SpriteInstance::SpriteInstance() {
        vao = new gl::VAO();
        vbo = new gl::VBO();

        vao->bind();
        vbo->bind();
        vbo->createVertexAttribPointer(2, GL_FLOAT, 4*sizeof(float), (void*)0);
        vbo->createVertexAttribPointer(2, GL_FLOAT, 4*sizeof(float), (void*)(2*sizeof(float)));
        vbo->unbind();
        vao->unbind();

        data = new SpriteData();
    }

    SpriteInstance::~SpriteInstance() {
        delete data;
        delete vbo;
        delete vao;
    }

    void SpriteInstance::bind() {
        //  what's this for?
        vao->bind();
    }

    void SpriteInstance::unbind() {
        gl::VAO::unbind();
    }

    void SpriteInstance::bufferVerts(size_t vertsize, float *verts) {
        vao->bind();
        vbo->bind();
        vbo->bufferVerts(vertsize, verts);
        vbo->unbind();
    }

    void SpriteInstance::draw(int triangles) {
        glDrawArrays(GL_TRIANGLES, 0, triangles * 3);
    }




    //  [DRAW]

    void InitialiseDrawmodes() {
        //  load draw modes (shaders)
        //  static gl::Shader *shaderSpriteSheet, *shaderSpriteSheetInvert, *shaderUI;
        
        glm::vec2 scrRes = glm::vec2((float)drawWidth, (float)drawHeight);

        // shaderSpriteSheetInvert = new gl::Shader();
        // shaderSpriteSheetInvert->load("/data/shaders/spritesheet.vert", "/data/shaders/spritesheet_invert.frag");
        // shaderSpriteSheetInvert->use();
        // shaderSpriteSheetInvert->setInt("txUnit", 0);
        // shaderSpriteSheetInvert->setVec2("res", scrRes);

        shaderSpriteSheet = new gl::Shader();
        shaderSpriteSheet->load("./data/shaders/spritesheet.vert", "./data/shaders/spritesheet.frag");
        shaderSpriteSheet->use();
        shaderSpriteSheet->setInt("txUnit", 0);
        shaderSpriteSheet->setVec2("res", scrRes);

        shader3d = new gl::Shader();
        shader3d->load("./data/shaders/model.vert", "./data/shaders/model.frag");
        
        pshader = new gl::Shader();
        pshader->load("./data/shaders/test.vert", "./data/shaders/test.frag");
        pshader->use();
        pshader->setInt("screenTexture", 0);
    }

    void SetDrawmode(Drawmode dmode) {
        if(dmode != currentDrawmode) {
            switch(dmode) {
                case DrawmodeSprite:
                    shaderSpriteSheet->use();
                    currentDrawmode = DrawmodeSprite;
                    glDisable(GL_DEPTH_TEST);
                    break;
                // case DrawmodeSpriteInvert:
                //     shaderSpriteSheetInvert->use();
                //     currentDrawmode = DrawmodeSpriteInvert;
                //     glDisable(GL_DEPTH_TEST);
                //     break;
                case Drawmode3D:
                    shader3d->use();
                    currentDrawmode = Drawmode3D;
                    glEnable(GL_DEPTH_TEST);
                    break;
                case DrawmodeUI:
                    pshader->use();
                    currentDrawmode = DrawmodeUI;
                    glDisable(GL_DEPTH_TEST);
                    break;
                default:
                    break;
            }
        }
    }

    //  configure the resolution setting of the current drawmode
    void ConfigureDrawmodeSpriteResolution(int x, int y) {
        glm::vec2 scrRes = glm::vec2((float)x, (float)y);
        shaderSpriteSheet->setVec2("res", scrRes);
    }

    void ConfigureDrawmodeSpriteTexture(int txunit) {
        shaderSpriteSheet->setInt("txUnit", txunit);
    }

    void ConfigureDrawmodeUITexture(int txunit) {
        pshader->setInt("screenTexture", txunit);
    }

    Camera3D::Camera3D() {
        angle_h = 0.f;
        angle_v = 0.f;
        mov_fw = 0.f;
        mov_up = 0.f;
        mov_lf = 0.f;
        mov_dir_fw = 0.f;
        mov_dir_lf = 0.f;

        dir_x = glm::vec3(1.f, 0.f, 0.f);
        dir_y = glm::vec3(0.f, 1.f, 0.f);
        dir_z = glm::vec3(0.f, 0.f, 1.f);

        //  todo add settings to this shit
        projection = glm::perspective(glm::radians(90.f), 640.f / 480.f, 0.1f, 100.f);
        eye = glm::vec3(0.f, 0.f, 0.f);
        direction = glm::vec3(sin(glm::radians(angle_h)), sin(glm::radians(angle_v)), cos(glm::radians(angle_h)));
        direction = glm::normalize(direction);
        view = glm::lookAt(eye, eye + direction, dir_y);
    }

    void Camera3D::update() {
        eye += dir_x * mov_fw;
        eye += dir_y * mov_up;
        eye += dir_z * mov_lf;

        //  cap up and down so far
        //  need to do trig on this shit for the up and down looking, fuck
        //  do it later
        if(angle_v > 90.f) angle_v = 90.f;
        if(angle_v < -90.f) angle_v = -90.f;

        direction = glm::vec3(sin(glm::radians(angle_h)), sin(glm::radians(angle_v)), cos(glm::radians(angle_h)));
        direction = glm::normalize(direction);

        eye += direction * mov_dir_fw;
        eye += glm::vec3(direction.z, 0.f, -direction.x) * mov_dir_lf;

        view = glm::lookAt(eye, eye + direction, dir_y);

        mov_fw = 0.f;
        mov_up = 0.f;
        mov_lf = 0.f;

        mov_dir_fw = 0.f;
        mov_dir_lf = 0.f;

        if(bound == this) {
            shader3d->setMat4("view", view);
            engine::shader3d->setVec3("viewPos", eye.x, eye.y, eye.z);
        }
    }

    void Camera3D::bind() {
        bound = this;
        // shader3d temp
        shader3d->setMat4("projection", projection);
        shader3d->setMat4("view", view);
    }

    void Camera3D::updateShaderPosition() {
        engine::shader3d->setVec3("viewPos", eye.x, eye.y, eye.z);
    }

    ModelInstance::ModelInstance() {
        modelmat = glm::mat4(1.f);
    }

    void ModelInstance::translate(float x, float y, float z) {
        modelmat = glm::translate(modelmat, glm::vec3(x, y, z));
    }

    void ModelInstance::scale(float x, float y, float z) {
        modelmat = glm::scale(modelmat, glm::vec3(x, y, z));
    }

    void ModelInstance::rotate(float degrees, float axis_x, float axis_y, float axis_z) {
        modelmat = glm::rotate(modelmat, glm::radians(degrees), glm::vec3(axis_x, axis_y, axis_z));
    }

    void ModelInstance::bind() {
        shader3d->setMat4("model", modelmat);
    }

    //  load settings from ini file
    //  [INIT]
    bool init(const char *title, int flags, int width, int height, const char *settingsPath) {
        debug_init();

        //  default controls if there's none in config
        controls[jump] = kb::Space;

        std::string inputStrings[] = {
            "Jump"
        };

        std::ifstream file;
        std::string settings;
        bool readstate = false;

        try {
            file.open(settingsPath);
            std::stringstream filestream;
            filestream << file.rdbuf();
            file.close();
            settings = filestream.str();
            readstate = true;
        } catch(std::ifstream::failure &ex) {
            engine::log_debug("failed to open settings file, %s\n%s",  strerror(errno), ex.what());
            file.close();
            // return false;
            readstate = false;
        }
        //  default settings
        //  really gotta put these somewhere else
        bool vsync = true;


        if(readstate) {
            ini_t *ini = ini_load(settings.c_str(), NULL);
            int vsync_i = ini_find_property(ini, INI_GLOBAL_SECTION, "vsync", 0);
            std::string vsync_t = ini_property_value(ini, INI_GLOBAL_SECTION, vsync_i);
            
            vsync = vsync_t == "true";
        }

        //  else use defaults

        if(vsync) {
            flags = flags | ENGINE_INIT_VSYNC;
        }


        init(title, flags, width, height);
        return true;
    }

    void init(const char *title, int flags, int width, int height) {
        init(title, flags, width, height, width, height);
    }

    //  2d init test
    void init(const char *title, int flags, int width, int height, int dwidth, int dheight) {
        debug_init();

        /*
            flags contains a list of init flags
            - resizeable
            - vsync
            -
            width can be fixed window width, suggested width or an aspect ratio based on selected screen init flags
        
        */

        glfwInit();
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        glfwWindowHint(GLFW_RESIZABLE, flags & ENGINE_INIT_RESIZEABLE);

        controls[jump] = kb::Space;

        for(int i = 0; i < kb::KeycodesLength; i++) {
            keyState[i] = 0;
        }

        drawWidth = dwidth;
        drawHeight = dheight;
        fps = 0u;
        ticks = glfwGetTime();
        frameTimeTicks = ticks;
        winflags = flags;

        const GLFWvidmode *dmode = glfwGetVideoMode(glfwGetPrimaryMonitor());
        if(winflags & ENGINE_INIT_TRUEFULLSCREEN) {
            maximised = true;
            glfwWindowHint(GLFW_RED_BITS, dmode->redBits);
            glfwWindowHint(GLFW_GREEN_BITS, dmode->greenBits);
            glfwWindowHint(GLFW_BLUE_BITS, dmode->blueBits);
            if(winflags & ENGINE_INIT_FIXEDFPS) {
                glfwWindowHint(GLFW_REFRESH_RATE, _ENGINE_FPS_CAP);
            } else {
                glfwWindowHint(GLFW_REFRESH_RATE, dmode->refreshRate);
            }
            gl::window = glfwCreateWindow(width, height, title, glfwGetPrimaryMonitor(), NULL);
            glfwSetWindowMonitor(gl::window, glfwGetPrimaryMonitor(), 0, 0, dmode->width, dmode->height, GLFW_DONT_CARE);
            scrWidth = width;
            scrHeight = height;
        } else if(winflags & ENGINE_INIT_BORDERLESS) {
            maximised = true;
            glfwWindowHint(GLFW_RED_BITS, dmode->redBits);
            glfwWindowHint(GLFW_GREEN_BITS, dmode->greenBits);
            glfwWindowHint(GLFW_BLUE_BITS, dmode->blueBits);
            glfwWindowHint(GLFW_REFRESH_RATE, dmode->refreshRate);
            gl::window = glfwCreateWindow(dmode->width, dmode->height, title, glfwGetPrimaryMonitor(), NULL);
            glfwSetWindowMonitor(gl::window, glfwGetPrimaryMonitor(), 0, 0, dmode->width, dmode->height, GLFW_DONT_CARE);
            scrWidth = dmode->width;
            scrHeight = dmode->height;
        } else {
            gl::window = glfwCreateWindow(width, height, title, NULL, NULL);
            scrWidth = width;
            scrHeight = height;
        }

        glfwMakeContextCurrent(gl::window);
        gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
        glfwSetKeyCallback(gl::window, key_callback);
        glfwSetWindowSizeCallback(gl::window, windowResizeCallback);
        glfwSetWindowMaximizeCallback(gl::window, windowMaximiseCallback);
        glfwSetErrorCallback(errorCallback);

        aspect_w = drawWidth;
        aspect_h = drawHeight;
        aspectRatio(&aspect_w, &aspect_h);

        if(winflags & ENGINE_INIT_FIXEDASPECT) {
            glfwSetWindowAspectRatio(gl::window, aspect_w, aspect_h);
        }

        if(winflags & ENGINE_INIT_VSYNC) {
            _vsync = true;
            glfwSwapInterval(1);
        } else {
            _vsync = false;
            glfwSwapInterval(0);

            next_time = std::chrono::high_resolution_clock::now() + std::chrono::microseconds(_ENGINE_NOVSYNC_DELAY_MICROSECONDS);
            // next_time = std::chrono::high_resolution_clock::now();
            #ifdef _MSC_VER
            timeBeginPeriod(1);
            #endif
        }

        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        // glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        // stbi_set_flip_vertically_on_load(true);  // don't need this because the shader i wrote accounts for it

        //  attempt to load data file
        assets = assetsys_create(0);
        assetsys_error_t error;
        error = assetsys_mount(assets, "data.zip", "/data");
        if(error != ASSETSYS_SUCCESS) {
            log_debug("mount of data file failed with error code %d\n", error);
            loadFromZip = false;
        } else {
            log_debug("mounted data file\n");
            loadFromZip = true;
        }

        
        InitialiseDrawmodes(); 
        SetDrawmode(DrawmodeSprite);

        int w, h;
        glfwGetFramebufferSize(gl::window, &w, &h);
        windowResizeCallback(gl::window, w, h);

        loadedModels = new std::unordered_map<std::string, ManagedModel*>();

        last_frame = glfwGetTime();
        deltatime = glfwGetTime() - last_frame;
        last_frame = glfwGetTime();


        //  imgui
        #ifndef IMGUI_DISABLE
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();

        ImGui::StyleColorsDark();

        ImGui_ImplGlfw_InitForOpenGL(gl::window, true);
        ImGui_ImplOpenGL3_Init("#version 330");

        imgui_windows = new std::vector<std::pair<std::string, std::vector<imgui_t>*>>();
        #endif
    }

    void windowMaximiseCallback(GLFWwindow *window, int m) {
        maximised = m == 1? true : false;
    }

    //  run this after window resizing
    void windowResizeCallback(GLFWwindow *window, int width, int height) {
        scrWidth = width;
        scrHeight = height;
        //  precalculate stuff for setviewport
        //  set viewport to specified rectangle (inside draw area)
        //  need to calculate x and y based off of the existing draw area
        scalex = (float)scrWidth / (float)drawWidth;
        scaley = (float)scrHeight / (float)drawHeight;

        if((winflags & ENGINE_INIT_FIXEDDRAWSIZE) || (winflags & ENGINE_INIT_FIXEDASPECT && maximised)) {
            
            
            scalex = (float)scrWidth / (float)drawWidth;
            scaley = (float)scrHeight / (float)drawHeight;
            
            float draw_ratio = (float)drawWidth / (float)drawHeight;
            float screen_ratio = (float)scrWidth / (float)scrHeight;
            if(draw_ratio > screen_ratio) {
                //  draw area is wider than screen
                float y_scale = (float)scrWidth / (float)drawWidth;
                float height = (float)drawHeight * y_scale;
                int offset = (scrHeight - (int)height) / 2;
                glViewport(0, offset, scrWidth, (int)height);
                viewport[0] = 0;
                viewport[1] = offset;
                viewport[2] = scrWidth;
                viewport[3] = (int)height;
                scaley = scalex;

                if(!(winflags & ENGINE_INIT_FIXEDDRAWSIZE)) {
                    drawWidth = scrWidth;
                    drawHeight = scrHeight - offset;
                }
            } else if(draw_ratio < screen_ratio) {
                //  draw area is narrower than screen
                float x_scale = (float)scrHeight / (float)drawHeight;
                float width = (float)drawWidth * x_scale;
                int offset = (scrWidth - (int)width) / 2;
                glViewport(offset, 0, (int)width, scrHeight);
                viewport[0] = offset;
                viewport[1] = 0;
                viewport[2] = (int)width;
                viewport[3] = scrHeight;
                scalex = scaley;

                if(!(winflags & ENGINE_INIT_FIXEDDRAWSIZE)) {
                    drawWidth = scrWidth - offset;
                    drawHeight = scrHeight;
                }
            } else {
                //  no letterboxing
                glViewport(0, 0, scrWidth, scrHeight);
                viewport[0] = 0;
                viewport[1] = 0;
                viewport[2] = scrWidth;
                viewport[3] = scrHeight;
            }

            

            
        } else {
            //  no letterboxing
            glViewport(0, 0, scrWidth, scrHeight);
            viewport[0] = 0;
            viewport[1] = 0;
            viewport[2] = scrWidth;
            viewport[3] = scrHeight;
            
            drawWidth = width;
            drawHeight = height;
        }

        log_debug("resize callback\n");
    }

    //  [FLIP]

    void flip() {       

        using namespace std::chrono;
        #ifdef _MSG_DEBUG_ENABLED_FPS
        uint32_t slept;
        #endif
        //  if vsync disabled, cap fps
        int temp = 0;
        if(!_vsync) {
            //  wait
            #ifdef _MSG_DEBUG_ENABLED_FPS
            high_resolution_clock::time_point sleep = high_resolution_clock::now();
            #endif
            #ifdef __GNUG__
            next_time += microseconds(_ENGINE_NOVSYNC_DELAY_MICROSECONDS);
            timespec delayt, delayr;
            nanoseconds delaym = duration_cast<nanoseconds>(next_time - high_resolution_clock::now());
            delayt.tv_sec = 0;
            delayt.tv_nsec = delaym.count();
            delayr.tv_nsec = 0;
            do {
                nanosleep(&delayt, &delayr);
            } while (delayr.tv_nsec > 0);
            #else
            next_time = high_resolution_clock::now() + microseconds(_ENGINE_NOVSYNC_DELAY_MICROSECONDS);
            std::this_thread::sleep_until(next_time);
            #endif

            #ifdef _MSG_DEBUG_ENABLED_FPS
            slept = duration_cast<milliseconds>(high_resolution_clock::now() - sleep).count();
            #endif

            // auto wake_time = steady_clock::now();
            while(high_resolution_clock::now() < next_time) {
                //  spin
                temp++;
            }
        }

        #ifdef _MSG_DEBUG_ENABLED_FPS
        //  print debug fps data
            
        static std::stringstream d;
        
        #ifndef IMGUI_DISABLE
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        for(size_t i = 0; i < imgui_windows->size(); i++) {
            ImGui::Begin(imgui_windows->at(i).first.c_str());
            for(size_t j = 0; j < imgui_windows->at(i).second->size(); j++) {
                imgui_t temp = imgui_windows->at(i).second->at(j);
                if(temp.type == 1) {
                    //  float
                    ImGui::Text(temp.text.c_str());
                    ImGui::SameLine();
                    if(temp.edit) {
                        ImGui::InputFloat("##value", (float*)temp.value, 1.0f);
                    } else {
                        ImGui::Text("%f", *(float*)temp.value);
                    }
                } else if(temp.type == 2) {
                    //  int
                    ImGui::Text(temp.text.c_str());
                    ImGui::SameLine();
                    if(temp.edit) {
                        ImGui::InputInt("##value", (int*)temp.value, 1.0f);
                    } else {
                        ImGui::Text("%d", *(int*)temp.value);
                    }
                } else if(temp.type == 3) {
                    //  bool
                    ImGui::Text(temp.text.c_str());
                    ImGui::SameLine();
                    if(*(bool*)temp.value) {
                        ImGui::Text(" True");
                    } else {
                        ImGui::Text(" False");
                    }
                } else if(temp.type == 4) {
                    //  double
                    ImGui::Text(temp.text.c_str());
                    ImGui::SameLine();
                    if(temp.edit) {
                        ImGui::InputDouble("##value", (double*)temp.value, 1.0f);
                    } else {
                        ImGui::Text("%f", *(double*)temp.value);
                    }
                }
            }
            ImGui::End();
        }

        ImGui::Begin("Performance");
        ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
        ImGui::Text(d.str().c_str());
        ImGui::End();

        ImGui::Render();
        setViewport();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        #endif

        double temp_ticks = glfwGetTime();
        avgTicksTotal += temp_ticks - frameTimeTicks;
        if(temp_ticks > ticks + 1.0) {
            d.str("");
            d << "Avg Frame time: " << avgTicksTotal / fps << "ms | ";
            d << "FPS: " << fps << " | Avg FPS: " << 1. / (avgTicksTotal / fps) << " | Deltatime: " << deltatime;
            if(!_vsync) {
                d << " | Slept: " << slept << "ms | ";
                d << "Spun: " << temp;
            }
            // glfwSetWindowTitle(gl::window, d.str().c_str());
            fps = 0u;
            ticks = temp_ticks;
            temp = 0u;
            avgTicksTotal = 0.;
        }
        frameTimeTicks = temp_ticks;
        ++fps;
        #endif




        //  update deltatime
        deltatime = glfwGetTime() - last_frame;
        last_frame = glfwGetTime();

        //  flip buffers
        glfwSwapBuffers(gl::window);

        //  clear new buffer
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
   
    }

    void close() {
        assetsys_dismount(assets, "data.zip", "/data");
        #ifndef IMGUI_DISABLE
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        #endif
        #ifdef _MSC_VER
        if(!_vsync) timeEndPeriod(1);
        #endif
        glfwDestroyWindow(gl::window);
        glfwTerminate();
    }

    void setViewport() {
        //  no arguments resets the viewport to original
        //  glviewport runs off of window resolution
        glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
        //  auto change draw res if appropriate flags?
    }

    void setViewport(int x, int y, int w, int h) {
        //  arguments provided in draw size
        //  sets coordinates based on window size
        //  scalex/scaley calculated on window size
        glViewport( viewport[0] + (int)(scalex * (float)x),
                    viewport[1] + (int)(scaley * (float)y),
                    (int)(scalex * (float)w),
                    (int)(scaley * (float)h));
    }

    void setDrawsize(int w, int h) {
        //  set size for sprite shader
        //  this sets the effective draw resolution that gets scaled to viewport resolution
        glm::vec2 scrRes = glm::vec2((float)w, (float)h);
        shaderSpriteSheet->setVec2("res", scrRes);
    }

    bool checkKey(int key) {
        return keyState[controls[key]] == GLFW_PRESS || keyState[controls[key]] == GLFW_REPEAT;
    }

    bool checkKeyPressed(int key) {
        return keyPressed[controls[key]];
    }

    void mouseCapture() {
        glfwSetInputMode(gl::window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        if(glfwRawMouseMotionSupported()) {
            glfwSetInputMode(gl::window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
        }
    }

    void mouseRelease() {
        glfwSetInputMode(gl::window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    }

    void errorCallback(int error, const char *description) {
        log_debug("Error %d: %s\n", error, description);
    }

    //  [IMGUI]

    void registerDebugVariable(std::string text, float *val, bool edit) {
        #ifndef IMGUI_DISABLE
        imgui_t icon;
        icon.text = text;
        icon.type = 1;
        icon.value = (void*)val;
        icon.edit = edit;
        if(imgui_windows) {
            if(imgui_windows->size() > 0) {
                imgui_windows->at(0).second->push_back(icon);
            }
        }
        #endif
    }

    void registerDebugVariable(std::string text, float *val, bool edit, size_t window) {
        #ifndef IMGUI_DISABLE
        imgui_t icon;
        icon.text = text;
        icon.type = 1;
        icon.value = (void*)val;
        icon.edit = edit;
        if(window < imgui_windows->size()) {
            if(imgui_windows->size() > window) {
                imgui_windows->at(window).second->push_back(icon);
            }
        }
        #endif
    }

    void registerDebugVariable(std::string text, double *val, bool edit) {
        #ifndef IMGUI_DISABLE
        imgui_t icon;
        icon.text = text;
        icon.type = 4;
        icon.value = (void*)val;
        icon.edit = edit;
        if(imgui_windows) {
            if(imgui_windows->size() > 0) {
                imgui_windows->at(0).second->push_back(icon);
            }
        }
        #endif
    }

    void registerDebugVariable(std::string text, double *val, bool edit, size_t window) {
        #ifndef IMGUI_DISABLE
        imgui_t icon;
        icon.text = text;
        icon.type = 4;
        icon.value = (void*)val;
        icon.edit = edit;
        if(window < imgui_windows->size()) {
            if(imgui_windows->size() > window) {
                imgui_windows->at(window).second->push_back(icon);
            }
        }
        #endif
    }

    void registerDebugVariable(std::string text, int *val, bool edit) {
        #ifndef IMGUI_DISABLE
        imgui_t icon;
        icon.text = text;
        icon.type = 2;
        icon.value = (void*)val;
        icon.edit = edit;
        if(imgui_windows) {
            if(imgui_windows->size() > 0) {
                imgui_windows->at(0).second->push_back(icon);
            }
        }
        #endif
    }

    void registerDebugVariable(std::string text, int *val, bool edit, size_t window) {
        #ifndef IMGUI_DISABLE
        imgui_t icon;
        icon.text = text;
        icon.type = 2;
        icon.value = (void*)val;
        icon.edit = edit;
        if(imgui_windows) {
            if(imgui_windows->size() > window) {
                imgui_windows->at(window).second->push_back(icon);
            }
        }
        #endif
    }

    void registerDebugVariable(std::string text, bool *val, bool edit) {
        #ifndef IMGUI_DISABLE
        imgui_t icon;
        icon.text = text;
        icon.type = 3;
        icon.value = (bool*)val;
        icon.edit = edit;
        if(imgui_windows) {
            if(imgui_windows->size() > 0) {
                imgui_windows->at(0).second->push_back(icon);
            }
        }
        #endif
    }

    void registerDebugVariable(std::string text, bool *val, bool edit, size_t window) {
        #ifndef IMGUI_DISABLE
        imgui_t icon;
        icon.text = text;
        icon.type = 3;
        icon.value = (bool*)val;
        icon.edit = edit;
        if(imgui_windows) {
            if(imgui_windows->size() > window) {
                imgui_windows->at(window).second->push_back(icon);
            }
        }
        #endif
    }

    void registerDebugWindow(std::string text) {
        #ifndef IMGUI_DISABLE
        std::vector<imgui_t> *v = new std::vector<imgui_t>();
        std::pair<std::string, std::vector<imgui_t>*> t = std::make_pair(text, v);
        imgui_windows->push_back(t);
        #endif
    }
}