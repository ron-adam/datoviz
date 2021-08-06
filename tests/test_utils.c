#include "../include/datoviz/alloc.h"
#include "../include/datoviz/common.h"
// #include "../include/datoviz/transforms.h"
// #include "../src/transforms_utils.h"
#include "tests.h"



/*************************************************************************************************/
/*  Typedefs                                                                                     */
/*************************************************************************************************/

typedef struct TestObject TestObject;
typedef struct _mvp _mvp;



/*************************************************************************************************/
/*  Structs                                                                                      */
/*************************************************************************************************/

struct TestObject
{
    DvzObject obj;
    float x;
};



struct _mvp
{
    mat4 model;
    mat4 view;
    mat4 proj;
};



/*************************************************************************************************/
/*  Common tests                                                                                 */
/*************************************************************************************************/

int test_utils_container(TestContext* tc)
{
    uint32_t capacity = 2;

    DvzContainer container = dvz_container(capacity, sizeof(TestObject), 0);
    AT(container.items != NULL);
    AT(container.item_size == sizeof(TestObject));
    AT(container.capacity == capacity);
    AT(container.count == 0);

    // Allocate one object.
    TestObject* a = dvz_container_alloc(&container);
    AT(a != NULL);
    a->x = 1;
    dvz_obj_created(&a->obj);
    AT(container.items[0] != NULL);
    AT(container.items[0] == a);
    AT(container.items[1] == NULL);
    AT(container.capacity == capacity);
    AT(container.count == 1);

    // Allocate another one.
    TestObject* b = dvz_container_alloc(&container);
    AT(b != NULL);
    b->x = 2;
    dvz_obj_created(&b->obj);
    AT(container.items[1] != NULL);
    AT(container.items[1] == b);
    AT(container.capacity == capacity);
    AT(container.count == 2);

    // Destroy the first object.
    dvz_obj_destroyed(&a->obj);

    // Allocate another one.
    TestObject* c = dvz_container_alloc(&container);
    AT(c != NULL);
    c->x = 3;
    dvz_obj_created(&c->obj);
    AT(container.items[0] != NULL);
    AT(container.items[0] == c);
    AT(container.capacity == capacity);
    AT(container.count == 2);

    // Allocate another one.
    // Container will be reallocated.
    TestObject* d = dvz_container_alloc(&container);
    AT(d != NULL);
    d->x = 4;
    dvz_obj_created(&d->obj);
    AT(container.capacity == 4);
    AT(container.count == 3);
    AT(container.items[2] != NULL);
    AT(container.items[2] == d);
    AT(container.items[3] == NULL);

    for (uint32_t k = 0; k < 10; k++)
    {
        DvzContainerIterator iter = dvz_container_iterator(&container);
        uint32_t i = 0;
        TestObject* obj = NULL;
        // Iterate through items.
        while (iter.item != NULL)
        {
            obj = iter.item;
            AT(obj != NULL);
            if (i == 0)
                AT(obj->x == 3);
            if (i == 1)
                AT(obj->x == 2);
            if (i == 2)
                AT(obj->x == 4);
            i++;
            dvz_container_iter(&iter);
        }
        ASSERT(i == 3);
    }

    // Destroy all objects.
    dvz_obj_destroyed(&b->obj);
    dvz_obj_destroyed(&c->obj);
    dvz_obj_destroyed(&d->obj);

    // Free all memory. This function will fail if there is at least one object not destroyed.
    dvz_container_destroy(&container);
    return 0;
}



static void* _thread_callback(void* user_data)
{
    ASSERT(user_data != NULL);
    dvz_sleep(10);
    *((int*)user_data) = 42;
    log_debug("from thread");
    return NULL;
}

int test_utils_thread(TestContext* tc)
{
    int data = 0;
    DvzThread thread = dvz_thread(_thread_callback, &data);
    AT(data == 0);
    dvz_thread_join(&thread);
    AT(data == 42);
    return 0;
}



/*************************************************************************************************/
/*  Alloc tests                                                                                  */
/*************************************************************************************************/

int test_utils_alloc_1(TestContext* tc)
{
    VkDeviceSize size = 16;
    DvzAlloc alloc = dvz_alloc(size, 0);
    DvzAllocSlot* slot = NULL;

    // WARNING: manipulating DvzAllocSlot* pointers is unsafe, because the underlying array may be
    // resized and therefore the pointers would become invalid. The public API of the Alloc never
    // deals with DvzAllocSlot* pointers, these are only for strictly internal use.

    // Initially, a single empty slot.
    AT(alloc.items.item_count == 1);

    slot = _get_slot(&alloc, 0);
    ASSERT(slot != NULL);
    AT(!slot->occupied);
    AT(slot->offset == 0);

    AT(_slot_idx(&alloc, slot) == 0);
    AT(_slot_size(&alloc, slot) == size);

    AT(_is_slot_available(&alloc, slot, 1));
    AT(_is_slot_available(&alloc, slot, size - 1));
    AT(_is_slot_available(&alloc, slot, size));
    AT(!_is_slot_available(&alloc, slot, size + 1));
    AT(!_is_slot_available(&alloc, slot, size * 2));

    AT(_find_slot_available(&alloc, 1) == slot);
    AT(_find_slot_available(&alloc, size - 1) == slot);
    AT(_find_slot_available(&alloc, size) == slot);
    AT(_find_slot_available(&alloc, size + 1) == NULL);
    AT(_find_slot_available(&alloc, size * 2) == NULL);

    AT(_last_slot(&alloc) == slot);
    AT(_next_slot(&alloc, slot) == NULL);


    // New occupied slot after the first which is empty.
    log_debug("insert new slot");
    _insert_slot_after(&alloc, slot, size / 2, true);
    slot = _get_slot(&alloc, 0);
    // [--------|xxxxxxxx]

    // DvzAllocSlot* first_slot = _get_slot(&alloc, 0);
    DvzAllocSlot* second_slot = _get_slot(&alloc, size / 2);
    ASSERT(second_slot != NULL);

    // First slot.
    AT(!slot->occupied);
    AT(slot->offset == 0);

    AT(_slot_idx(&alloc, slot) == 0);
    AT(_slot_size(&alloc, slot) == size / 2);

    AT(_is_slot_available(&alloc, slot, 1));
    AT(!_is_slot_available(&alloc, slot, size - 1));
    AT(_is_slot_available(&alloc, slot, size / 2));
    AT(!_is_slot_available(&alloc, slot, size / 2 + 1));

    AT(_find_slot_available(&alloc, 1) == slot);
    AT(_find_slot_available(&alloc, size / 2 - 1) == slot);
    AT(_find_slot_available(&alloc, size / 2) == slot);
    AT(_find_slot_available(&alloc, size / 2 + 1) == NULL);

    AT(_next_slot(&alloc, slot) == second_slot);


    // Second slot.
    AT(second_slot->occupied);
    AT(second_slot->offset == size / 2);

    AT(_slot_idx(&alloc, second_slot) == 1);
    AT(_slot_size(&alloc, second_slot) == size / 2);

    AT(!_is_slot_available(&alloc, second_slot, 1));

    AT(_find_slot_available(&alloc, 1) == slot);
    AT(_find_slot_available(&alloc, size / 2 - 1) == slot);
    AT(_find_slot_available(&alloc, size / 2) == slot);
    AT(_find_slot_available(&alloc, size / 2 + 1) == NULL);

    AT(_last_slot(&alloc) == second_slot);
    AT(_next_slot(&alloc, slot) == second_slot);


    // Double alloc size.
    _double_alloc_size(&alloc);
    // [--------|xxxxxxxx|----------------]

    DvzAllocSlot* third_slot = _get_slot(&alloc, size);
    ASSERT(third_slot != NULL);

    AT(_next_slot(&alloc, slot) == second_slot);
    AT(_last_slot(&alloc) == third_slot);

    AT(_find_slot_available(&alloc, size / 2) == slot);
    AT(_find_slot_available(&alloc, size / 2 + 1) == third_slot);
    AT(_find_slot_available(&alloc, size) == third_slot);
    AT(_find_slot_available(&alloc, size + 1) == NULL);


    dvz_alloc_destroy(&alloc);
    return 0;
}



int test_utils_alloc_2(TestContext* tc)
{
    VkDeviceSize size = 64;
    VkDeviceSize alignment = 4;
    VkDeviceSize offset = 0;
    VkDeviceSize resized = 0;

    DvzAlloc alloc = dvz_alloc(size, alignment);

    offset = dvz_alloc_new(&alloc, 2, &resized);
    AT(offset == 0);
    AT(!resized);

    offset = dvz_alloc_new(&alloc, 2, &resized);
    AT(offset == 4);
    AT(!resized);

    dvz_alloc_destroy(&alloc);
    return 0;
}



/*************************************************************************************************/
/* Transform tests                                                                               */
/*************************************************************************************************/

// int test_utils_transforms_1(TestContext* tc)
// {
//     const uint32_t n = 100000;
//     const double eps = 1e-3;

//     // Compute the data bounds of an array of dvec3.
//     DvzArray pos_in = dvz_array(n, DVZ_DTYPE_DOUBLE);
//     double* positions = (double*)pos_in.data;
//     for (uint32_t i = 0; i < n; i++)
//         positions[i] = -5 + 10 * dvz_rand_float();

//     DvzBox box = _box_bounding(&pos_in);
//     AT(fabs(box.p0[0] + 5) < eps);
//     AT(fabs(box.p1[0] - 5) < eps);

//     box = _box_cube(box);
//     // AT(fabs(box.p0[0] + 2.5) < eps);
//     // AT(fabs(box.p1[0] - 7.5) < eps);
//     // AT(fabs(box.p0[1] - 3.5) < eps);
//     // AT(fabs(box.p1[1] - 13.5) < eps);
//     AT(fabs(box.p0[0] + 5) < eps);
//     AT(fabs(box.p1[0] - 5) < eps);


//     // // Normalize the data.
//     // DvzArray pos_out = dvz_array(n, DVZ_DTYPE_DOUBLE);
//     // _transform_linear(box, &pos_in, DVZ_BOX_NDC, &pos_out);
//     // positions = (double*)pos_out.data;
//     // double* pos = NULL;
//     // double v = 0;
//     // for (uint32_t i = 0; i < n; i++)
//     // {
//     //     pos = dvz_array_item(&pos_out, i);
//     //     //     v = (*pos)[0];
//     //     //     AT(-1 <= v && v <= +1);
//     //     //     v = (*pos)[1];
//     //     //     AT(-1 <= v && v <= +1);
//     //     v = (*pos);
//     //     AT(-1 - eps <= v && v <= +1 + eps);
//     // }

//     dvz_array_destroy(&pos_in);
//     // dvz_array_destroy(&pos_out);

//     return 0;
// }



// int test_utils_transforms_2(TestContext* tc)
// {
//     DvzBox box0 = {{0, 0, -1}, {10, 10, 1}};
//     DvzTransform tr = _transform_interp(box0, DVZ_BOX_NDC);

//     {
//         dvec3 in = {0, 0, -1};
//         dvec3 out = {0};
//         _transform_apply(&tr, in, out);
//         for (uint32_t i = 0; i < 3; i++)
//             AT(out[i] == -1);
//     }

//     {
//         dvec3 in = {10, 10, 1};
//         dvec3 out = {0};
//         _transform_apply(&tr, in, out);
//         for (uint32_t i = 0; i < 3; i++)
//             AT(out[i] == +1);
//     }

//     tr = _transform_inv(&tr);

//     {
//         dvec3 in = {-1, -1, -1};
//         dvec3 out = {0};
//         _transform_apply(&tr, in, out);
//         AT(out[0] == 0);
//         AT(out[1] == 0);
//         AT(out[2] == -1);
//     }

//     {
//         dvec3 in = {1, 1, 1};
//         dvec3 out = {0};
//         _transform_apply(&tr, in, out);
//         AT(out[0] == 10);
//         AT(out[1] == 10);
//         AT(out[2] == 1);
//     }

//     return 0;
// }



// int test_utils_transforms_3(TestContext* tc)
// {
//     DvzMVP mvp = {0};

//     glm_mat4_identity(mvp.model);
//     glm_mat4_identity(mvp.view);
//     glm_mat4_identity(mvp.proj);

//     glm_rotate(mvp.model, M_PI, (vec3){0, 1, 0});

//     DvzTransform tr = _transform_mvp(&mvp);

//     dvec3 in = {0.5, 0.5, 0};
//     dvec3 out = {0};
//     _transform_apply(&tr, in, out);
//     AC(out[0], -.5, EPS);
//     AC(out[1], -.5, EPS);
//     AC(out[2], +.5, EPS);

//     return 0;
// }



// int test_utils_transforms_4(TestContext* tc)
// {
//     DvzBox box0 = {{0, 0, -1}, {10, 10, 1}};
//     DvzBox box1 = {{0, 0, 0}, {1, 2, 3}};
//     DvzTransform tr = _transform_interp(box0, DVZ_BOX_NDC);

//     DvzTransformChain tch = _transforms();
//     _transforms_append(&tch, tr);
//     tr = _transform_interp(DVZ_BOX_NDC, box1);
//     _transforms_append(&tch, tr);

//     {
//         dvec3 in = {10, 10, 1}, out = {0};
//         _transforms_apply(&tch, in, out);
//         AT(out[0] == 1);
//         AT(out[1] == 2);
//         AT(out[2] == 3);
//     }

//     {
//         dvec3 in = {1, 2, 3}, out = {0};
//         tch = _transforms_inv(&tch);
//         _transforms_apply(&tch, in, out);
//         AT(out[0] == 10);
//         AT(out[1] == 10);
//         AT(out[2] == 1);
//     }

//     return 0;
// }



// DO NOT UNCOMMENT:

// int test_utils_transforms_5(TestContext* tc)
// {
//     DvzApp* app = dvz_app(DVZ_BACKEND_GLFW);
//     DvzGpu* gpu = dvz_gpu_best(app);
//     DvzCanvas* canvas = dvz_canvas(gpu, TEST_WIDTH, TEST_HEIGHT, 0);
//     DvzGrid grid = dvz_grid(canvas, 2, 4);
//     DvzPanel* panel = dvz_panel(&grid, 1, 2);

//     dvz_app_run(app, 3);

//     panel->data_coords.box.p0[0] = 0;
//     panel->data_coords.box.p0[1] = 0;
//     panel->data_coords.box.p0[2] = -1;

//     panel->data_coords.box.p1[0] = 10;
//     panel->data_coords.box.p1[1] = 20;
//     panel->data_coords.box.p1[2] = 1;

//     dvec3 in, out;
//     in[0] = 5;
//     in[1] = 10;
//     in[2] = 0;

//     dvz_transform(panel, DVZ_CDS_DATA, in, DVZ_CDS_SCENE, out);
//     AC(out[0], 0, EPS);
//     AC(out[1], 0, EPS);
//     AC(out[2], 0, EPS);

//     dvz_transform(panel, DVZ_CDS_DATA, in, DVZ_CDS_VULKAN, out);
//     AC(out[0], 0, EPS);
//     AC(out[1], 0, EPS);
//     AC(out[2], 0.5, EPS);

//     in[0] = 0;
//     in[1] = 20;
//     in[2] = 0;
//     uvec2 size = {0};

//     dvz_canvas_size(canvas, DVZ_CANVAS_SIZE_FRAMEBUFFER, size);
//     dvz_transform(panel, DVZ_CDS_DATA, in, DVZ_CDS_FRAMEBUFFER, out);
//     AC(out[0], size[0] / 2., EPS);
//     AC(out[1], size[1] / 2., EPS);
//     AC(out[2], 0.5, EPS);

//     dvz_canvas_size(canvas, DVZ_CANVAS_SIZE_SCREEN, size);
//     dvz_transform(panel, DVZ_CDS_DATA, in, DVZ_CDS_WINDOW, out);
//     AC(out[0], size[0] / 2., EPS);
//     AC(out[1], size[1] / 2., EPS);
//     AC(out[2], 0.5, EPS);

//     TEST_END
// }



/*************************************************************************************************/
/* Colormap tests                                                                                */
/*************************************************************************************************/

int test_utils_colormap_idx(TestContext* tc)
{
    DvzColormap cmap = DVZ_CMAP_BLUES;
    uint8_t value = 128;
    cvec2 ij = {0};

    dvz_colormap_idx(cmap, value, ij);
    AT(ij[0] == (int)cmap);
    AT(ij[1] == value);

    return 0;
}



int test_utils_colormap_uv(TestContext* tc)
{
    DvzColormap cmap = DVZ_CMAP_BLUES;
    DvzColormap cpal32 = DVZ_CPAL032_PAIRED;
    DvzColormap cpal = DVZ_CPAL256_GLASBEY;
    uint8_t value = 128;
    vec2 uv = {0};
    float eps = .01;

    dvz_colormap_uv(cmap, value, uv);
    AC(uv[0], .5, .05);
    AC(uv[1], (int)cmap / 256.0, .05);

    dvz_colormap_uv(cpal, value, uv);
    AC(uv[0], .5, .05);
    AC(uv[1], (int)cpal / 256.0, .05);

    dvz_colormap_uv(cpal32, value, uv);
    AC(uv[0], .7520, eps);
    AC(uv[1], .9395, eps);

    return 0;
}



int test_utils_colormap_extent(TestContext* tc)
{
    DvzColormap cmap = DVZ_CMAP_BLUES;
    DvzColormap cpal32 = DVZ_CPAL032_PAIRED;
    DvzColormap cpal = DVZ_CPAL256_GLASBEY;
    vec4 uvuv = {0};
    float eps = .01;

    dvz_colormap_extent(cmap, uvuv);
    AC(uvuv[0], 0, eps);
    AC(uvuv[2], 1, eps);
    AC(uvuv[1], .029, eps);
    AC(uvuv[3], .029, eps);

    dvz_colormap_extent(cpal, uvuv);
    AC(uvuv[0], 0, eps);
    AC(uvuv[2], 1, eps);
    AC(uvuv[1], .69, eps);
    AC(uvuv[3], .69, eps);

    dvz_colormap_extent(cpal32, uvuv);
    AC(uvuv[0], .25, eps);
    AC(uvuv[2], .37, eps);
    AC(uvuv[1], .94, eps);
    AC(uvuv[3], .94, eps);

    return 0;
}



int test_utils_colormap_default(TestContext* tc)
{
    DvzColormap cmap = DVZ_CMAP_HSV;
    cvec4 color = {0};
    cvec4 expected = {0, 0, 0, 255};

    dvz_colormap(cmap, 0, color);
    expected[0] = 255;
    AEn(4, color, expected);

    dvz_colormap(cmap, 128, color);
    expected[0] = 0;
    expected[1] = 255;
    expected[2] = 245;
    AEn(4, color, expected);

    dvz_colormap(cmap, 255, color);
    expected[0] = 255;
    expected[1] = 0;
    expected[2] = 23;
    AEn(4, color, expected);

    return 0;
}



int test_utils_colormap_scale(TestContext* tc)
{
    DvzColormap cmap = DVZ_CMAP_HSV;
    cvec4 color = {0};
    cvec4 expected = {0, 0, 0, 255};
    float vmin = -1;
    float vmax = +1;

    dvz_colormap_scale(cmap, -1, vmin, vmax, color);
    expected[0] = 255;
    expected[1] = 0;
    expected[2] = 0;
    AEn(4, color, expected);

    dvz_colormap_scale(cmap, 0, vmin, vmax, color);
    expected[0] = 0;
    expected[1] = 255;
    expected[2] = 245;
    AEn(4, color, expected);

    dvz_colormap_scale(cmap, 1, vmin, vmax, color);
    expected[0] = 255;
    expected[1] = 0;
    expected[2] = 23;
    AEn(4, color, expected);

    return 0;
}



int test_utils_colormap_packuv(TestContext* tc)
{
    vec2 uv = {0};

    dvz_colormap_packuv((cvec3){10, 20, 30}, uv);
    AT(uv[1] == -1);
    AT(uv[0] == 10 + 256 * 20 + 256 * 256 * 30);

    return 0;
}



int test_utils_colormap_array(TestContext* tc)
{
    DvzColormap cmap = DVZ_CMAP_BLUES;
    double vmin = -1;
    double vmax = +1;
    cvec4 color = {0};

    uint32_t count = 100;
    double* values = calloc(count, sizeof(double));
    for (uint32_t i = 0; i < count; i++)
        values[i] = -1.0 + 2.0 * i / (double)(count - 1);

    cvec4* colors = calloc(count, sizeof(cvec4));
    dvz_colormap_array(cmap, count, values, vmin, vmax, colors);
    for (uint32_t i = 0; i < count; i++)
    {
        dvz_colormap_scale(cmap, values[i], vmin, vmax, color);
        AEn(4, color, colors[i])
    }

    FREE(values);
    FREE(colors);

    return 0;
}
