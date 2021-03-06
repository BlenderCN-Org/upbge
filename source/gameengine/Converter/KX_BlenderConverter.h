/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file KX_BlenderConverter.h
 *  \ingroup bgeconv
 */

#ifndef __KX_BLENDERCONVERTER_H__
#define __KX_BLENDERCONVERTER_H__

#include <map>
#include <vector>

#ifdef _MSC_VER  // MSVC doesn't support incomplete type in std::unique_ptr.
#  include "KX_BlenderMaterial.h"
#  include "RAS_MeshObject.h"

#  include "KX_BlenderScalarInterpolator.h"
#endif

#include "CM_Thread.h"

class CStringValue;
class KX_BlenderSceneConverter;
class KX_KetsjiEngine;
class KX_LibLoadStatus;
class KX_BlenderMaterial;
class BL_InterpolatorList;
class SCA_IActuator;
class SCA_IController;
class RAS_MeshObject;
class RAS_Rasterizer;
struct Main;
struct BlendHandle;
struct Mesh;
struct Scene;
struct Material;
struct bAction;
struct bActuator;
struct bController;
struct TaskPool;
struct Depsgraph;

template<class Value> using UniquePtrList = std::vector<std::unique_ptr<Value>>;

class KX_BlenderConverter {
 private:
  class SceneSlot {
   public:
    UniquePtrList<KX_BlenderMaterial> m_materials;
    UniquePtrList<RAS_MeshObject> m_meshobjects;
    UniquePtrList<BL_InterpolatorList> m_interpolators;

    std::map<bAction *, BL_InterpolatorList *> m_actionToInterp;

    SceneSlot();
    SceneSlot(const KX_BlenderSceneConverter &converter);
    ~SceneSlot();

    void Merge(SceneSlot &other);
    void Merge(const KX_BlenderSceneConverter &converter);
  };

  std::map<KX_Scene *, SceneSlot> m_sceneSlots;

  struct ThreadInfo {
    TaskPool *m_pool;
    CM_ThreadMutex m_mutex;
  } m_threadinfo;

  // Saved KX_LibLoadStatus objects
  std::map<std::string, KX_LibLoadStatus *> m_status_map;
  std::vector<KX_LibLoadStatus *> m_mergequeue;

  Main *m_maggie;
  std::vector<Main *> m_DynamicMaggie;

  KX_KetsjiEngine *m_ketsjiEngine;
  bool m_alwaysUseExpandFraming;

 public:
  KX_BlenderConverter(Main *maggie, KX_KetsjiEngine *engine);
  virtual ~KX_BlenderConverter();

  /** \param Scenename name of the scene to be converted.
   * \param destinationscene pass an empty scene, everything goes into this
   * \param dictobj python dictionary (for pythoncontrollers)
   */
  void ConvertScene(KX_Scene *destinationscene,
                    RAS_Rasterizer *rasty,
                    RAS_ICanvas *canvas,
                    bool libloading);
  void RemoveScene(KX_Scene *scene);

  void SetAlwaysUseExpandFraming(bool to_what);

  void RegisterInterpolatorList(KX_Scene *scene,
                                BL_InterpolatorList *interpolator,
                                bAction *for_act);
  BL_InterpolatorList *FindInterpolatorList(KX_Scene *scene, bAction *for_act);

  Scene *GetBlenderSceneForName(const std::string &name);
  CListValue<CStringValue> *GetInactiveSceneNames();

  Main *GetMain();
  Main *CreateMainDynamic(const std::string &path);
  Main *GetMainDynamicPath(const std::string &path) const;
  const std::vector<Main *> &GetMainDynamic() const;

  KX_LibLoadStatus *LinkBlendFileMemory(void *data,
                                        int length,
                                        const char *path,
                                        char *group,
                                        KX_Scene *scene_merge,
                                        char **err_str,
                                        short options);
  KX_LibLoadStatus *LinkBlendFilePath(
      const char *path, char *group, KX_Scene *scene_merge, char **err_str, short options);
  KX_LibLoadStatus *LinkBlendFile(BlendHandle *bpy_openlib,
                                  const char *path,
                                  char *group,
                                  KX_Scene *scene_merge,
                                  char **err_str,
                                  short options);

  bool FreeBlendFile(Main *maggie);
  bool FreeBlendFile(const std::string &path);

  RAS_MeshObject *ConvertMeshSpecial(KX_Scene *kx_scene, Main *maggie, const std::string &name);

  void MergeScene(KX_Scene *to, KX_Scene *from);

  void MergeAsyncLoads();
  void FinalizeAsyncLoads();
  void AddScenesToMergeQueue(KX_LibLoadStatus *status);

  void PrintStats();

  // LibLoad Options.
  enum {
    LIB_LOAD_LOAD_ACTIONS = 1,
    LIB_LOAD_VERBOSE = 2,
    LIB_LOAD_LOAD_SCRIPTS = 4,
    LIB_LOAD_ASYNC = 8,
  };
};

#endif  // __KX_BLENDERCONVERTER_H__
