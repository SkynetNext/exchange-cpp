#!/usr/bin/env python3
"""
生成低延迟可观测性架构图 (Tracing Layer + Collection Layer)。
输出到 docs/images/observability_architecture.png
风格参考 AWS 架构图：圆角矩形、分组背景、清晰箭头与标签。
依赖: pip install matplotlib
"""

import os
import matplotlib.pyplot as plt
from matplotlib.patches import FancyBboxPatch, FancyArrowPatch
import matplotlib.patheffects as path_effects

# ============================================================================
# 风格常量 (AWS-style, 顶流级)
# ============================================================================
BG_COLOR = "#fafafa"
GROUP_BG_TRACING = "#e8f4fc"
GROUP_BG_COLLECTION = "#fef6e8"
GROUP_BORDER = "#c9dae8"
PROCESS_BG = "#f0f8ff"           # 进程分组背景（更浅的蓝）
PROCESS_BORDER = "#87ceeb"
BOX_BG = "#ffffff"
BOX_BORDER = "#232f3e"
BOX_SHADOW = "#d0d0d0"
DATA_COLOR = "#0073bb"
AERON_COLOR = "#0073bb"
SHARED_MEM_COLOR = "#ec7211"
REPORT_COLOR = "#545b64"
FONT_NAME = "sans-serif"
TITLE_SIZE = 16
LAYER_LABEL_SIZE = 12
LABEL_SIZE = 10
SMALL_LABEL = 9
DPI = 250
FIG_W, FIG_H = 14, 8.5
BOX_RADIUS = 0.12
SHADOW_OFFSET = 0.04


def draw_box_with_shadow(ax, x, y, w, h, label_lines, radius=BOX_RADIUS):
    """绘制带阴影的圆角矩形框。"""
    shadow = FancyBboxPatch(
        (x + SHADOW_OFFSET, y - SHADOW_OFFSET), w, h,
        boxstyle=f"round,pad=0,rounding_size={radius}",
        facecolor=BOX_SHADOW, edgecolor="none", zorder=1
    )
    ax.add_patch(shadow)
    box = FancyBboxPatch(
        (x, y), w, h,
        boxstyle=f"round,pad=0,rounding_size={radius}",
        facecolor=BOX_BG, edgecolor=BOX_BORDER, linewidth=1.5, zorder=2
    )
    ax.add_patch(box)
    mid_y = y + h / 2
    if len(label_lines) == 1:
        ax.text(x + w / 2, mid_y, label_lines[0], fontsize=LABEL_SIZE,
                ha="center", va="center", fontfamily=FONT_NAME, zorder=3)
    else:
        line_gap = 0.18
        top_y = mid_y + line_gap * (len(label_lines) - 1) / 2
        for i, line in enumerate(label_lines):
            ax.text(x + w / 2, top_y - i * line_gap, line, fontsize=SMALL_LABEL,
                    ha="center", va="center", fontfamily=FONT_NAME, zorder=3)
    return x, y, w, h


def draw_group_bg(ax, x, y, w, h, label, color, border_color, radius=0.2, label_pos="top"):
    """绘制分组背景。"""
    bg = FancyBboxPatch(
        (x, y), w, h,
        boxstyle=f"round,pad=0,rounding_size={radius}",
        facecolor=color, edgecolor=border_color, linewidth=1.2, zorder=0
    )
    ax.add_patch(bg)
    if label_pos == "top":
        ax.text(x + w / 2, y + h - 0.25, label, fontsize=LAYER_LABEL_SIZE,
                ha="center", va="top", fontweight="bold", color="#232f3e",
                fontfamily=FONT_NAME, zorder=1)
    else:
        ax.text(x + w / 2, y + h - 0.15, label, fontsize=SMALL_LABEL,
                ha="center", va="top", fontweight="normal", color="#555555",
                fontfamily=FONT_NAME, fontstyle="italic", zorder=1)


def draw_arrow(ax, start, end, color, style="solid", label=None, curved=False, label_offset=(0, 0.12)):
    """绘制箭头。"""
    if curved:
        dx = end[0] - start[0]
        dy = end[1] - start[1]
        if abs(dy) > abs(dx):
            rad = 0.25 if dx > 0 else -0.25
        else:
            rad = 0.15 if dy < 0 else -0.15
        conn_style = f"arc3,rad={rad}"
    else:
        conn_style = "arc3,rad=0"

    arrow = FancyArrowPatch(
        start, end,
        arrowstyle="-|>,head_width=0.12,head_length=0.08",
        connectionstyle=conn_style,
        color=color,
        linewidth=2.0 if style == "solid" else 1.8,
        linestyle="-" if style == "solid" else (0, (5, 3)),
        zorder=4,
        mutation_scale=12
    )
    ax.add_patch(arrow)

    if label:
        mid_x = (start[0] + end[0]) / 2 + label_offset[0]
        mid_y = (start[1] + end[1]) / 2 + label_offset[1]
        t = ax.text(mid_x, mid_y, label, fontsize=SMALL_LABEL - 0.5, ha="center", va="center",
                    color=color, fontfamily=FONT_NAME, fontweight="bold", zorder=5)
        t.set_path_effects([
            path_effects.Stroke(linewidth=3, foreground="white"),
            path_effects.Normal()
        ])


def main():
    script_dir = os.path.dirname(os.path.abspath(__file__))
    docs_dir = os.path.dirname(script_dir)
    images_dir = os.path.join(docs_dir, "images")
    os.makedirs(images_dir, exist_ok=True)
    out_path = os.path.join(images_dir, "observability_architecture")

    fig, ax = plt.subplots(1, 1, figsize=(FIG_W, FIG_H), facecolor=BG_COLOR)
    ax.set_facecolor(BG_COLOR)
    ax.set_aspect("equal")
    ax.axis("off")
    ax.set_xlim(0, 14)
    ax.set_ylim(0, 8.5)

    # ========================================================================
    # 布局参数
    # ========================================================================
    tracing_y = 4.5
    tracing_h = 3.4
    collection_y = 0.6
    collection_h = 3.0

    box_h = 0.85
    box_w = 1.9
    box_w_mid = 2.2

    # ========================================================================
    # Tracing Layer
    # ========================================================================
    draw_group_bg(ax, 0.5, tracing_y, 13, tracing_h, "Tracing Layer",
                  GROUP_BG_TRACING, GROUP_BORDER)

    # --- Gateway Process 子分组 (包含左右两个框) ---
    gw_process_x = 1.0
    gw_process_w = 5.2
    gw_process_y = tracing_y + 0.4
    gw_process_h = 2.5
    draw_group_bg(ax, gw_process_x, gw_process_y, gw_process_w, gw_process_h,
                  "Gateway Process (same host)", PROCESS_BG, PROCESS_BORDER, radius=0.15, label_pos="bottom")

    tracing_box_y = gw_process_y + 0.5
    mid_y_tracing = tracing_box_y + box_h / 2

    # Box 1: Gateway + Trading (T1, T2)
    x1 = gw_process_x + 0.3
    draw_box_with_shadow(ax, x1, tracing_box_y, box_w, box_h,
                         ["Gateway (T1 in)", "Trading (T2 out)"])
    p1_right = x1 + box_w
    p1_center = x1 + box_w / 2

    # Box 3: Clearing + Gateway (T5, T6) - 在同一进程分组内
    x3 = x1 + box_w + 0.5
    draw_box_with_shadow(ax, x3, tracing_box_y, box_w, box_h,
                         ["Clearing (T5 in)", "Gateway (T6 out)"])
    p3_right = x3 + box_w
    p3_center = x3 + box_w / 2

    # --- Matching Engine (独立进程) ---
    me_process_x = gw_process_x + gw_process_w + 1.5
    me_process_w = 3.0
    me_process_h = 2.5
    draw_group_bg(ax, me_process_x, gw_process_y, me_process_w, me_process_h,
                  "Matching Process", PROCESS_BG, PROCESS_BORDER, radius=0.15, label_pos="bottom")

    x2 = me_process_x + (me_process_w - box_w_mid) / 2
    draw_box_with_shadow(ax, x2, tracing_box_y, box_w_mid, box_h,
                         ["Matching Engine", "(T3 in / T4 out)"])
    p2_left = x2
    p2_right = x2 + box_w_mid
    p2_center = x2 + box_w_mid / 2

    # --- Aeron 箭头 ---
    # Gateway Process → Matching Engine
    arrow1_start = (p3_right + 0.1, mid_y_tracing)
    arrow1_end = (p2_left - 0.1, mid_y_tracing)
    draw_arrow(ax, arrow1_start, arrow1_end, AERON_COLOR, label="Aeron", label_offset=(0, 0.22))

    # Matching Engine → Gateway Process (返回)
    arrow2_start = (p2_left - 0.1, mid_y_tracing - 0.25)
    arrow2_end = (p3_right + 0.1, mid_y_tracing - 0.25)
    draw_arrow(ax, arrow2_start, arrow2_end, AERON_COLOR, label="Aeron", label_offset=(0, -0.22))

    # 内部流向箭头 (T2 out → T5 in 的概念，用虚线表示同进程内流转)
    ax.annotate("", xy=(x3 - 0.05, mid_y_tracing), xytext=(p1_right + 0.05, mid_y_tracing),
                arrowprops=dict(arrowstyle="-|>", color="#666666", lw=1.5, ls=(0, (4, 2))))
    ax.text((p1_right + x3) / 2, mid_y_tracing + 0.18, "in-process", fontsize=8,
            ha="center", va="bottom", color="#666666", fontstyle="italic")

    # ========================================================================
    # Collection Layer
    # ========================================================================
    draw_group_bg(ax, 0.5, collection_y, 13, collection_h, "Collection Layer",
                  GROUP_BG_COLLECTION, "#f5deb3")

    coll_box_y = collection_y + 1.0
    coll_box_h = 0.85
    coll_box_w = 1.8
    agg_box_w = 2.2
    mid_y_coll = coll_box_y + coll_box_h / 2

    # Agent G (对应 Gateway Process)
    xg = 2.0
    draw_box_with_shadow(ax, xg, coll_box_y, coll_box_w, coll_box_h, ["Agent G"])
    pg_center = xg + coll_box_w / 2
    pg_right = xg + coll_box_w
    pg_top = coll_box_y + coll_box_h

    # Agent M (对应 Matching Process)
    xm = 6.0
    draw_box_with_shadow(ax, xm, coll_box_y, coll_box_w, coll_box_h, ["Agent M"])
    pm_center = xm + coll_box_w / 2
    pm_right = xm + coll_box_w
    pm_top = coll_box_y + coll_box_h

    # Aggregator
    xa = 10.0
    draw_box_with_shadow(ax, xa, coll_box_y, agg_box_w, coll_box_h, ["Aggregator"])
    pa_left = xa

    # ========================================================================
    # Shared Memory 箭头 (Tracing → Collection)
    # ========================================================================
    # Gateway Process → Agent G (从进程分组底部到 Agent G 顶部)
    gw_process_bottom = gw_process_y
    gw_process_center = gw_process_x + gw_process_w / 2
    draw_arrow(ax, (gw_process_center, gw_process_bottom), (pg_center, pg_top),
               SHARED_MEM_COLOR, style="dashed", label="Shared Memory", curved=False, label_offset=(-1.0, 0.1))

    # Matching Process → Agent M
    me_process_bottom = gw_process_y
    me_process_center = me_process_x + me_process_w / 2
    draw_arrow(ax, (me_process_center, me_process_bottom), (pm_center, pm_top),
               SHARED_MEM_COLOR, style="dashed", label="Shared Memory", curved=False, label_offset=(1.0, 0.1))

    # ========================================================================
    # Reporting 箭头 (Agents → Aggregator)
    # ========================================================================
    draw_arrow(ax, (pg_right + 0.08, mid_y_coll + 0.08), (pa_left - 0.08, mid_y_coll + 0.08),
               REPORT_COLOR, label="", label_offset=(0, 0))
    draw_arrow(ax, (pm_right + 0.08, mid_y_coll - 0.08), (pa_left - 0.08, mid_y_coll - 0.08),
               REPORT_COLOR, label="", label_offset=(0, 0))
    ax.text((pm_right + pa_left) / 2, mid_y_coll + 0.4, "Reporting", fontsize=SMALL_LABEL,
            ha="center", va="center", color=REPORT_COLOR, fontweight="bold", fontfamily=FONT_NAME)

    # ========================================================================
    # 标题
    # ========================================================================
    fig.suptitle("Low Latency Observability Architecture",
                 fontsize=TITLE_SIZE, fontweight="bold", y=0.97, fontfamily=FONT_NAME, color="#232f3e")

    plt.tight_layout(rect=[0, 0, 1, 0.95])
    plt.savefig(out_path + ".png", dpi=DPI, bbox_inches="tight", facecolor=BG_COLOR, edgecolor="none")
    plt.close()
    print(f"图表已保存到: {out_path}.png")


if __name__ == "__main__":
    main()
