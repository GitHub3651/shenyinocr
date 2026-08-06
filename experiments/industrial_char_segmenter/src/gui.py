"""Tkinter GUI for simple industrial character segmentation runs."""

from __future__ import annotations

import os
import queue
import subprocess
import sys
import threading
from datetime import datetime
from pathlib import Path
from tkinter import (
    BooleanVar,
    Button,
    Checkbutton,
    Entry,
    Frame,
    IntVar,
    Label,
    LabelFrame,
    StringVar,
    TclError,
    Tk,
    filedialog,
    messagebox,
)
from tkinter.scrolledtext import ScrolledText
from tkinter.ttk import Combobox, Progressbar

from .provider_presets import (
    DEFAULT_PROVIDER_ID,
    get_models,
    get_provider,
    get_provider_display_name,
    get_provider_id_by_display_name,
    provider_ids,
)


PROJECT_ROOT = Path(__file__).resolve().parents[1]


def _default_output_dir() -> str:
    stamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    return str(PROJECT_ROOT / "data" / "output" / f"run_{stamp}")


def _open_path(path: str) -> None:
    target = Path(path)
    if not target.exists():
        raise FileNotFoundError(f"Path does not exist: {target}")
    if sys.platform.startswith("win"):
        os.startfile(str(target))  # type: ignore[attr-defined]
    elif sys.platform == "darwin":
        subprocess.Popen(["open", str(target)])
    else:
        subprocess.Popen(["xdg-open", str(target)])


class SegmenterGUI:
    def __init__(self, root: Tk):
        self.root = root
        self.root.title("工业字符自动分割")
        self.root.geometry("940x660")
        self.root.minsize(860, 600)

        self.image_path = StringVar()
        self.target_text = StringVar()
        self.output_dir = StringVar(value=_default_output_dir())
        self.model_json_path = StringVar()
        self.api_key = StringVar(value=os.environ.get("VLM_API_KEY", ""))
        self.api_workspace_id = StringVar(
            value=os.environ.get("VLM_API_WORKSPACE_ID", "")
            or os.environ.get("DASHSCOPE_WORKSPACE_ID", "")
        )
        initial_provider = os.environ.get("VLM_PROVIDER", DEFAULT_PROVIDER_ID)
        if initial_provider not in provider_ids():
            initial_provider = DEFAULT_PROVIDER_ID
        self.api_provider = StringVar(value=initial_provider)
        self.provider_display = StringVar(value=get_provider_display_name(initial_provider))
        self.api_model = StringVar(value=os.environ.get("VLM_API_MODEL", ""))
        self.provider_note = StringVar()
        self.current_model_text = StringVar()
        self.connection_status = StringVar(value="未测试")
        self.progress_text = StringVar(value="进度：就绪")
        self.progress_value = IntVar(value=0)
        self.api_timeout = IntVar(value=180)
        self.max_retry = IntVar(value=0)
        self.auto_open_output = BooleanVar(value=True)
        self.status = StringVar(value="就绪")

        self._last_report: dict | None = None
        self._last_progress_stage: str | None = None
        self._worker: threading.Thread | None = None
        self._queue: queue.Queue[tuple[str, object]] = queue.Queue()
        self._provider_display_to_id = {
            get_provider_display_name(provider_id): provider_id
            for provider_id in provider_ids()
        }

        self._build_layout()
        self._refresh_models()
        self._bind_connection_state_traces()
        self.root.after(100, self._poll_queue)

    def _build_layout(self) -> None:
        outer = Frame(self.root, padx=14, pady=12)
        outer.pack(fill="both", expand=True)

        form = LabelFrame(outer, text="输入", padx=10, pady=10)
        form.pack(fill="x")

        self._path_row(form, 0, "图片", self.image_path, self._choose_image)
        self._entry_row(form, 1, "目标字符串", self.target_text)
        self._path_row(form, 2, "输出目录", self.output_dir, self._choose_output_dir)
        self._combo_row(
            form,
            3,
            "模型厂商",
            self.provider_display,
            list(self._provider_display_to_id.keys()),
            self._on_provider_changed,
        )
        self._combo_row(form, 4, "模型", self.api_model, [], None)
        self._entry_row(form, 5, "API Key", self.api_key, show="*")
        self._entry_row(form, 6, "千问业务空间ID（可选）", self.api_workspace_id)
        self._path_row(
            form,
            7,
            "测试 bbox JSON（可选）",
            self.model_json_path,
            self._choose_model_json,
        )

        Label(
            form,
            textvariable=self.provider_note,
            fg="#555555",
            anchor="w",
            justify="left",
        ).grid(row=8, column=1, columnspan=2, sticky="ew", padx=(8, 0), pady=(0, 8))

        Label(form, text="当前使用").grid(row=9, column=0, sticky="w", pady=(8, 0))
        Label(
            form,
            textvariable=self.current_model_text,
            fg="#333333",
            anchor="w",
        ).grid(row=9, column=1, columnspan=2, sticky="ew", padx=(8, 0), pady=(8, 0))

        Label(form, text="连接状态").grid(row=10, column=0, sticky="w", pady=(8, 0))
        self.connection_label = Label(
            form,
            textvariable=self.connection_status,
            fg="#777777",
            anchor="w",
        )
        self.connection_label.grid(row=10, column=1, sticky="ew", padx=(8, 8), pady=(8, 0))
        self.test_button = Button(
            form,
            text="测试连接",
            width=10,
            command=self._test_connection,
        )
        self.test_button.grid(row=10, column=2, sticky="e", pady=(8, 0))

        Label(form, text="失败后重试次数").grid(row=11, column=0, sticky="w", pady=(8, 0))
        Entry(form, textvariable=self.max_retry, width=10).grid(
            row=11, column=1, sticky="w", pady=(8, 0)
        )
        Label(form, text="API 超时(秒)").grid(
            row=12, column=0, sticky="w", pady=(8, 0)
        )
        Entry(form, textvariable=self.api_timeout, width=10).grid(
            row=12, column=1, sticky="w", pady=(8, 0)
        )
        Checkbutton(
            form,
            text="完成后打开输出目录",
            variable=self.auto_open_output,
        ).grid(row=11, column=2, sticky="w", pady=(8, 0), padx=(8, 0))

        form.columnconfigure(1, weight=1)

        actions = Frame(outer, pady=10)
        actions.pack(fill="x")

        self.run_button = Button(actions, text="开始分割", width=14, command=self._run)
        self.run_button.pack(side="left")

        Button(actions, text="新输出目录", width=12, command=self._reset_output_dir).pack(
            side="left", padx=(8, 0)
        )
        Button(actions, text="打开输出目录", width=14, command=self._open_output_dir).pack(
            side="left", padx=(8, 0)
        )
        Button(actions, text="打开预览图", width=12, command=self._open_preview).pack(
            side="left", padx=(8, 0)
        )

        Label(actions, textvariable=self.status, anchor="e").pack(
            side="right", fill="x", expand=True
        )

        progress_frame = Frame(outer)
        progress_frame.pack(fill="x", pady=(0, 8))
        Label(progress_frame, textvariable=self.progress_text, anchor="w").pack(
            fill="x"
        )
        self.progress_bar = Progressbar(
            progress_frame,
            maximum=100,
            variable=self.progress_value,
            mode="determinate",
        )
        self.progress_bar.pack(fill="x", pady=(4, 0))

        log_frame = LabelFrame(outer, text="运行日志", padx=8, pady=8)
        log_frame.pack(fill="both", expand=True)
        self.log = ScrolledText(log_frame, height=14, wrap="word")
        self.log.pack(fill="both", expand=True)
        self.log.configure(state="disabled")

        self._append_log("选择图片、输入目标字符串，选择厂商和模型，填写 API Key 后点击开始分割。")
        self._append_log("如果选择测试 bbox JSON，则本次运行不会调用 API。")

    def _entry_row(
        self,
        parent: LabelFrame,
        row: int,
        label: str,
        variable: StringVar,
        show: str | None = None,
    ) -> None:
        Label(parent, text=label).grid(row=row, column=0, sticky="w", pady=4)
        Entry(parent, textvariable=variable, show=show).grid(
            row=row, column=1, sticky="ew", padx=(8, 8), pady=4
        )

    def _path_row(
        self,
        parent: LabelFrame,
        row: int,
        label: str,
        variable: StringVar,
        command,
    ) -> None:
        Label(parent, text=label).grid(row=row, column=0, sticky="w", pady=4)
        Entry(parent, textvariable=variable).grid(
            row=row, column=1, sticky="ew", padx=(8, 8), pady=4
        )
        Button(parent, text="浏览", width=8, command=command).grid(
            row=row, column=2, sticky="e", pady=4
        )

    def _combo_row(
        self,
        parent: LabelFrame,
        row: int,
        label: str,
        variable: StringVar,
        values: list[str],
        on_change,
    ) -> None:
        Label(parent, text=label).grid(row=row, column=0, sticky="w", pady=4)
        combo = Combobox(parent, textvariable=variable, values=values, state="readonly")
        combo.grid(row=row, column=1, sticky="ew", padx=(8, 8), pady=4)
        if on_change:
            combo.bind("<<ComboboxSelected>>", on_change)
        combo.bind("<<ComboboxSelected>>", self._reset_connection_status, add="+")
        if label == "模型":
            self.model_combo = combo

    def _on_provider_changed(self, _event=None) -> None:
        provider_id = get_provider_id_by_display_name(self.provider_display.get())
        self.api_provider.set(provider_id)
        provider = get_provider(provider_id)
        env_key = str(provider["api_key_env"])
        if not self.api_key.get().strip() and os.environ.get(env_key):
            self.api_key.set(os.environ[env_key])
        self._refresh_models()

    def _refresh_models(self) -> None:
        provider_id = self.api_provider.get() or DEFAULT_PROVIDER_ID
        models = get_models(provider_id)
        model_ids = [model["id"] for model in models]
        if hasattr(self, "model_combo"):
            self.model_combo.configure(values=model_ids)
        if self.api_model.get() not in model_ids:
            self.api_model.set(model_ids[0] if model_ids else "")
        provider = get_provider(provider_id)
        self.provider_note.set(
            f"{provider['display_name']}：{provider['note']} "
            "测试 bbox JSON 只用于离线调试；留空时会调用所选厂商 API。"
        )
        self._update_current_model_text()
        self._reset_connection_status()

    def _update_current_model_text(self) -> None:
        self.current_model_text.set(
            f"{self.provider_display.get()} / {self.api_model.get() or '未选择模型'}"
        )

    def _reset_connection_status(self, _event=None) -> None:
        self._update_current_model_text()
        if hasattr(self, "connection_label"):
            self.connection_status.set("未测试")
            self.connection_label.configure(fg="#777777")

    def _bind_connection_state_traces(self) -> None:
        for variable in (
            self.api_key,
            self.api_model,
            self.api_provider,
            self.api_workspace_id,
        ):
            variable.trace_add("write", lambda *_args: self._reset_connection_status())

    def _append_log(self, message: str) -> None:
        stamp = datetime.now().strftime("%H:%M:%S")
        self.log.configure(state="normal")
        self.log.insert("end", f"[{stamp}] {message}\n")
        self.log.see("end")
        self.log.configure(state="disabled")

    def _choose_image(self) -> None:
        path = filedialog.askopenfilename(
            title="选择工业图片",
            initialdir=str(PROJECT_ROOT / "data" / "input"),
            filetypes=[
                ("Image files", "*.bmp *.png *.jpg *.jpeg *.tif *.tiff"),
                ("All files", "*.*"),
            ],
        )
        if path:
            self.image_path.set(path)
            if not self.output_dir.get().strip():
                self._reset_output_dir()

    def _choose_model_json(self) -> None:
        path = filedialog.askopenfilename(
            title="选择测试 bbox JSON",
            initialdir=str(PROJECT_ROOT / "data" / "input"),
            filetypes=[("JSON files", "*.json"), ("All files", "*.*")],
        )
        if path:
            self.model_json_path.set(path)

    def _choose_output_dir(self) -> None:
        path = filedialog.askdirectory(
            title="选择输出目录",
            initialdir=str(PROJECT_ROOT / "data" / "output"),
        )
        if path:
            self.output_dir.set(path)

    def _reset_output_dir(self) -> None:
        self.output_dir.set(_default_output_dir())

    def _validate_form(self) -> bool:
        image = self.image_path.get().strip()
        target = self.target_text.get()
        output = self.output_dir.get().strip()
        model_json = self.model_json_path.get().strip()
        api_key = self.api_key.get().strip()
        api_provider = self.api_provider.get().strip()
        api_model = self.api_model.get().strip()

        if not image:
            messagebox.showerror("缺少图片", "请选择输入图片。")
            return False
        if not Path(image).exists():
            messagebox.showerror("图片不存在", image)
            return False
        if not target:
            messagebox.showerror("缺少目标字符串", "请输入目标字符串。")
            return False
        if not output:
            messagebox.showerror("缺少输出目录", "请选择输出目录。")
            return False
        if model_json and not Path(model_json).exists():
            messagebox.showerror("测试 bbox JSON 不存在", model_json)
            return False
        if not model_json:
            provider = get_provider(api_provider)
            if not provider["supports_image"]:
                messagebox.showerror(
                    "厂商不支持图片输入",
                    str(provider["note"]),
                )
                return False
            if not api_key:
                messagebox.showerror("缺少 API Key", "未选择测试 JSON 时，必须填写 API Key。")
                return False
            if not api_model:
                messagebox.showerror("缺少模型", "请选择视觉模型。")
                return False
        try:
            retry_count = int(self.max_retry.get())
        except (TclError, ValueError):
            messagebox.showerror("重试次数错误", "重试次数必须是非负整数。")
            return False
        if retry_count < 0:
            messagebox.showerror("重试次数错误", "重试次数必须是非负整数。")
            return False
        try:
            timeout = int(self.api_timeout.get())
        except (TclError, ValueError):
            messagebox.showerror("API 超时错误", "API 超时必须是正整数。")
            return False
        if timeout <= 0:
            messagebox.showerror("API 超时错误", "API 超时必须是正整数。")
            return False
        return True

    def _run(self) -> None:
        if self._worker and self._worker.is_alive():
            return
        if not self._validate_form():
            return

        self.run_button.configure(state="disabled")
        self.status.set("运行中...")
        self.progress_value.set(0)
        self.progress_text.set("进度：准备开始")
        self._last_progress_stage = None
        self._append_log("开始运行分割流程。")

        args = {
            "image_path": self.image_path.get().strip(),
            "target_text": self.target_text.get(),
            "output_dir": self.output_dir.get().strip(),
            "model_json_path": self.model_json_path.get().strip() or None,
            "max_retry": int(self.max_retry.get()),
            "api_key": self.api_key.get().strip() or None,
            "api_provider": self.api_provider.get().strip() or None,
            "api_base_url": None,
            "api_model": self.api_model.get().strip() or None,
            "api_workspace_id": self.api_workspace_id.get().strip() or None,
            "api_timeout": int(self.api_timeout.get()),
            "progress_callback": self._thread_progress_callback,
        }
        self._worker = threading.Thread(target=self._run_worker, args=(args,), daemon=True)
        self._worker.start()

    def _thread_progress_callback(self, stage: str, percent: int, message: str) -> None:
        self._queue.put(
            (
                "progress",
                {
                    "stage": stage,
                    "percent": max(0, min(100, int(percent))),
                    "message": message,
                },
            )
        )

    def _validate_api_for_test(self) -> bool:
        api_key = self.api_key.get().strip()
        api_provider = self.api_provider.get().strip()
        api_model = self.api_model.get().strip()
        provider = get_provider(api_provider)
        if not provider["supports_image"]:
            self._set_connection_failed(str(provider["note"]))
            return False
        if not api_model:
            self._set_connection_failed("请选择模型。")
            return False
        if not api_key:
            self._set_connection_failed("请先填写 API Key。")
            return False
        try:
            timeout = int(self.api_timeout.get())
        except (TclError, ValueError):
            self._set_connection_failed("API 超时必须是正整数。")
            return False
        if timeout <= 0:
            self._set_connection_failed("API 超时必须是正整数。")
            return False
        return True

    def _test_connection(self) -> None:
        if not self._validate_api_for_test():
            return
        self._update_current_model_text()
        self.connection_status.set("测试中...")
        self.connection_label.configure(fg="#9a6700")
        self.test_button.configure(state="disabled")
        args = {
            "api_key": self.api_key.get().strip(),
            "provider_id": self.api_provider.get().strip(),
            "model": self.api_model.get().strip(),
            "workspace_id": self.api_workspace_id.get().strip() or None,
            "timeout": int(self.api_timeout.get()),
        }
        threading.Thread(target=self._test_connection_worker, args=(args,), daemon=True).start()

    def _test_connection_worker(self, args: dict) -> None:
        try:
            from .vlm_client import RealVLMClient

            client = RealVLMClient(
                api_key=args["api_key"],
                provider_id=args["provider_id"],
                model=args["model"],
                workspace_id=args["workspace_id"],
                timeout=args["timeout"],
            )
            result = client.test_connection()
            self._queue.put(("connection_ok", result))
        except Exception as exc:
            self._queue.put(("connection_error", str(exc)))

    def _run_worker(self, args: dict) -> None:
        try:
            from .agent import run_char_segmentation_agent

            report = run_char_segmentation_agent(**args)
            self._queue.put(("report", report))
        except ModuleNotFoundError as exc:
            if exc.name == "cv2":
                self._queue.put(
                    (
                        "error",
                        "缺少 OpenCV 依赖 cv2。请在项目目录执行: pip install -r requirements.txt",
                    )
                )
            else:
                self._queue.put(("error", f"{type(exc).__name__}: {exc}"))
        except Exception as exc:
            self._queue.put(("error", f"{type(exc).__name__}: {exc}"))

    def _poll_queue(self) -> None:
        try:
            while True:
                kind, payload = self._queue.get_nowait()
                if kind == "report":
                    self._handle_report(payload)  # type: ignore[arg-type]
                elif kind == "error":
                    self._handle_error(str(payload))
                elif kind == "connection_ok":
                    self._handle_connection_ok(payload)  # type: ignore[arg-type]
                elif kind == "connection_error":
                    self._handle_connection_error(str(payload))
                elif kind == "progress":
                    self._handle_progress(payload)  # type: ignore[arg-type]
        except queue.Empty:
            pass
        self.root.after(100, self._poll_queue)

    def _handle_progress(self, payload: dict) -> None:
        percent = int(payload.get("percent", 0))
        message = str(payload.get("message", ""))
        self.progress_value.set(percent)
        self.progress_text.set(f"进度：{percent}% - {message}")
        self.status.set(message)
        stage = str(payload.get("stage", ""))
        if stage != self._last_progress_stage:
            self._append_log(f"进度 {percent}%：{message}")
            self._last_progress_stage = stage

    def _set_connection_failed(self, message: str) -> None:
        self.connection_status.set("连接失败")
        self.connection_label.configure(fg="#b42318")
        self._append_log(f"连接失败：{message}")

    def _handle_connection_ok(self, result: dict) -> None:
        self.test_button.configure(state="normal")
        self.current_model_text.set(f"{result['provider_name']} / {result['model']}")
        self.connection_status.set("连接成功")
        self.connection_label.configure(fg="#14863d")
        self._append_log(f"连接成功，正在使用模型：{result['provider_name']} / {result['model']}")

    def _handle_connection_error(self, message: str) -> None:
        self.test_button.configure(state="normal")
        self._set_connection_failed(message)
        messagebox.showerror("连接失败", message)

    def _handle_report(self, report: dict) -> None:
        self._last_report = report
        self.run_button.configure(state="normal")

        if report.get("status") == "success":
            crop_info = report.get("crop_info", {})
            file_count = crop_info.get("file_count", 0)
            self.status.set("完成")
            self.progress_value.set(100)
            self.progress_text.set("进度：100% - 完成")
            self._append_log(f"运行成功，输出字符数量: {file_count}")
            self._append_log(f"输出目录: {report.get('output_dir')}")
            warnings = report.get("warnings") or []
            if warnings:
                self._append_log(f"警告数量: {len(warnings)}")
            if self.auto_open_output.get():
                self._open_output_dir(silent=True)
            messagebox.showinfo("完成", f"分割完成，输出字符数量: {file_count}")
        else:
            self.status.set("失败")
            self.progress_text.set("进度：失败")
            errors = report.get("errors") or ["未知错误"]
            self._append_log("运行失败:")
            for error in errors:
                self._append_log(f"  {error}")
            messagebox.showerror("失败", "\n".join(str(e) for e in errors))

    def _handle_error(self, message: str) -> None:
        self.run_button.configure(state="normal")
        self.status.set("失败")
        self.progress_text.set("进度：失败")
        self._append_log(message)
        messagebox.showerror("错误", message)

    def _open_output_dir(self, silent: bool = False) -> None:
        path = self.output_dir.get().strip()
        if not path:
            if not silent:
                messagebox.showerror("缺少输出目录", "没有可打开的输出目录。")
            return
        try:
            _open_path(path)
        except Exception as exc:
            if not silent:
                messagebox.showerror("打开失败", str(exc))

    def _open_preview(self) -> None:
        preview = Path(self.output_dir.get().strip()) / "preview_boxes.png"
        try:
            _open_path(str(preview))
        except Exception as exc:
            messagebox.showerror("打开失败", str(exc))


def main() -> int:
    root = Tk()
    SegmenterGUI(root)
    root.mainloop()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
