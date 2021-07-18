#include "../include/datoviz/common.h"
#include "../src/ticks.h"
#include "tests.h"



/*************************************************************************************************/
/* Tick tests                                                                                    */
/*************************************************************************************************/

int test_utils_ticks_1(TestContext* context)
{
    DvzAxesContext ctx = {0};
    ctx.coord = DVZ_AXES_COORD_X;
    ctx.size_viewport = 1000;
    ctx.size_glyph = 10;

    DvzAxesTicks ticks = create_ticks(0, 1, 11, ctx);
    ticks.lmin_in = 0;
    ticks.lmax_in = 1;
    ticks.lstep = .1;
    const uint32_t N = tick_count(ticks.lmin_in, ticks.lmax_in, ticks.lstep);
    ticks.value_count = N;

    ticks.labels = calloc(N * MAX_GLYPHS_PER_TICK, sizeof(char));
    ticks.precision = 3;

    for (bool scientific = false; scientific; scientific = true)
    {
        ticks.format = scientific ? DVZ_TICK_FORMAT_SCIENTIFIC : DVZ_TICK_FORMAT_DECIMAL;
        make_labels(&ticks, &ctx, false);
        char* s = NULL;
        for (uint32_t i = 0; i < N; i++)
        {
            s = &ticks.labels[i * MAX_GLYPHS_PER_TICK];
            if (strlen(s) == 0)
                break;
            log_debug("%s ", s);
            if (i > 0)
            {
                if (scientific)
                {
                    AT(strchr(s, 'e') != NULL);
                }
                else
                {
                    AT(strchr(s, 'e') == NULL);
                }
            }
        }
    }

    FREE(ticks.labels);
    return 0;
}



int test_utils_ticks_2(TestContext* context)
{
    DvzAxesContext ctx = {0};
    ctx.coord = DVZ_AXES_COORD_X;
    ctx.size_viewport = 5000;
    ctx.size_glyph = 5;
    ctx.extensions = 0;

    double x = 1.23456;
    DvzAxesTicks ticks = dvz_ticks(x, x + 1e-8, ctx);
    AT(ticks.format == DVZ_TICK_FORMAT_DECIMAL);
    for (uint32_t i = 0; i < ticks.value_count; i++)
        log_debug("tick #%02d: %s", i, &ticks.labels[i * MAX_GLYPHS_PER_TICK]);
    return 0;
}



int test_utils_ticks_duplicate(TestContext* context)
{
    DvzAxesContext ctx = {0};
    ctx.coord = DVZ_AXES_COORD_X;
    ctx.size_viewport = 2000;
    ctx.size_glyph = 5;

    DvzAxesTicks ticks = dvz_ticks(-10.12, 20.34, ctx);
    for (uint32_t i = 0; i < ticks.value_count; i++)
        log_debug("tick #%02d: %s", i, &ticks.labels[i * MAX_GLYPHS_PER_TICK]);
    AT(!duplicate_labels(&ticks, &ctx));

    ticks = dvz_ticks(.001, .002, ctx);
    for (uint32_t i = 0; i < ticks.value_count; i++)
        log_debug("tick #%02d: %s", i, &ticks.labels[i * MAX_GLYPHS_PER_TICK]);
    AT(!duplicate_labels(&ticks, &ctx));

    ticks = dvz_ticks(-0.131456, -0.124789, ctx);
    for (uint32_t i = 0; i < ticks.value_count; i++)
        log_debug("tick #%02d: %s", i, &ticks.labels[i * MAX_GLYPHS_PER_TICK]);
    AT(!duplicate_labels(&ticks, &ctx));

    dvz_ticks_destroy(&ticks);
    return 0;
}



int test_utils_ticks_extend(TestContext* context)
{
    DvzAxesContext ctx = {0};
    ctx.coord = DVZ_AXES_COORD_X;
    ctx.size_viewport = 1000;
    ctx.size_glyph = 10;

    DvzAxesTicks ticks = {0};

    // No extensions.
    double x0 = -2.123, x1 = +2.456;
    ctx.extensions = 0;
    ticks = dvz_ticks(x0, x1, ctx);
    for (uint32_t i = 0; i < ticks.value_count; i++)
        log_debug("tick #%02d: %s", i, &ticks.labels[i * MAX_GLYPHS_PER_TICK]);

    // 1 extension on each side.
    ctx.extensions = 1;
    ticks = dvz_ticks(x0, x1, ctx);
    for (uint32_t i = 0; i < ticks.value_count; i++)
        log_debug("tick #%02d: %s", i, &ticks.labels[i * MAX_GLYPHS_PER_TICK]);

    // 2 extension on each side.
    ctx.extensions = 2;
    ticks = dvz_ticks(x0, x1, ctx);
    for (uint32_t i = 0; i < ticks.value_count; i++)
        log_debug(
            "tick #%02d: %s (%f)", i, &ticks.labels[i * MAX_GLYPHS_PER_TICK], ticks.values[i]);

    dvz_ticks_destroy(&ticks);

    return 0;
}
