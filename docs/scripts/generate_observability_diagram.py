#!/usr/bin/env python3
"""
生成低延迟可观测性架构图 (Tracing Layer + Collection Layer)。
输出到 docs/images/observability_architecture.png
依赖: pip install diagrams，且系统已安装 Graphviz (winget install graphviz.graphviz)。
"""

import os

from diagrams import Cluster, Diagram, Edge
from diagrams.onprem.compute import Server
from diagrams.onprem.monitoring import Grafana
from diagrams.onprem.network import Gunicorn

# 提高 DPI 使 PNG 文字更清晰（Graphviz 默认 96，文档用 200–300 为宜）
graph_attr = {
    "dpi": "200",
    "fontsize": "20",
    "bgcolor": "white",
}

def main():
    script_dir = os.path.dirname(os.path.abspath(__file__))
    docs_dir = os.path.dirname(script_dir)
    images_dir = os.path.join(docs_dir, "images")
    os.makedirs(images_dir, exist_ok=True)
    out_path = os.path.join(images_dir, "observability_architecture")

    with Diagram(
        "Low Latency Trading Architecture",
        show=False,
        direction="LR",
        graph_attr=graph_attr,
        filename=out_path,
    ):
        with Cluster("Tracing Layer"):
            with Cluster("GW + Trading + Clearing"):
                gw_in = Server("T1 / T2")
                gw_out = Server("T5 / T6")

            matching = Server("Matching (T3 / T4)")

            gw_in >> Edge(label="Aeron", color="blue") >> matching
            matching >> Edge(label="Aeron", color="blue") >> gw_out

        with Cluster("Collection Layer"):
            aggregator = Grafana("Aggregator")
            agent_g = Gunicorn("Agent G")
            agent_m = Gunicorn("Agent M")

            agent_g >> aggregator
            agent_m >> aggregator

        gw_in - Edge(color="darkorange", style="dashed", label="Shared Memory") - agent_g
        gw_out - Edge(color="darkorange", style="dashed", label="Shared Memory") - agent_g
        matching - Edge(color="darkorange", style="dashed", label="Shared Memory") - agent_m

        gw_out >> Edge(style="dotted", label="Reporting") >> aggregator

    print(f"图表已保存到: {out_path}.png")


if __name__ == "__main__":
    main()
