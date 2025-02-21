import tkinter as tk
from tkinter import scrolledtext
import subprocess
import threading

class ServerGUI:
    def __init__(self, master):
        self.master = master
        self.master.title("Wi-Fi Data Collector")
        self.master.geometry("600x450")  # 設定視窗的寬為600，高為500



        # Frame to contain Location Label, Entry, and Submit button with extra padding
        self.location_frame = tk.LabelFrame(master, relief='groove', bd=2, padx=10, pady=10, text='Label')
        self.location_frame.grid(row=0, column=0, columnspan=2, padx=(10, 5), pady=(10, 5), sticky='w')

        # Location Label & Entry in the same frame
        self.loc_label = tk.Label(self.location_frame, text="Location", font=('calibre', 10, 'bold'))
        self.loc_label.grid(row=0, column=0, padx=(5, 5), pady=(5, 5), sticky='w')

        self.location = tk.StringVar()
        self.loc_entry = tk.Entry(self.location_frame, textvariable=self.location)
        self.loc_entry.grid(row=0, column=1, padx=(5, 5), pady=(5, 5), sticky='w')

        self.submit_button = tk.Button(self.location_frame, text="Submit", command=self.submit_loc)
        self.submit_button.grid(row=0, column=2, padx=(50, 10), pady=(5, 5))

        # Frame for Start and Stop buttons
        self.button_frame = tk.LabelFrame(master, relief='groove', bd=2, padx=10, pady=10, text='Server Control')
        self.button_frame.grid(row=0, column=2, columnspan=3, padx=(5, 10), pady=(10, 5), sticky='e')

        self.start_button = tk.Button(self.button_frame, text="啟動伺服器", command=self.start_server)
        self.start_button.grid(row=0, column=0, padx=(5, 5), pady=(5, 5))

        self.stop_button = tk.Button(self.button_frame, text="停止伺服器", command=self.stop_server, state=tk.DISABLED)
        self.stop_button.grid(row=0, column=1, padx=(5, 5), pady=(5, 5))

        # Aligning status label across entire row for symmetry
        self.status_label = tk.Label(self.button_frame, text="伺服器狀態: 未啟動", fg="red")
        self.status_label.grid(row=1, column=0, columnspan=5, pady=(5, 10), sticky='ew')

        # Create Labelframe for the log window
        self.log_frame = tk.LabelFrame(master, text="Message Box")
        self.log_frame.grid(row=2, column=0, columnspan=5, padx=10, pady=(5, 10), sticky='ew')

        # Log window inside Labelframe
        self.log_window = scrolledtext.ScrolledText(self.log_frame, width=80, height=20, state='disabled')
        self.log_window.pack(padx=10, pady=10, fill='both', expand=True)

        # Configure weights for adaptive resizing
        for i in range(5):
            master.grid_columnconfigure(i, weight=1)


        # 初始化
        self.server_process = None
    
    def submit_loc(self):
        # 取得輸入的伺服器位置
        self.location_value = self.location.get()  # 將結果存到新變數

    def log_message(self, message):
        """新增訊息到日誌視窗"""
        self.log_window.config(state='normal')
        self.log_window.insert(tk.END, message + "\n")
        self.log_window.see(tk.END)
        self.log_window.config(state='disabled')

    def start_server(self):
        """啟動伺服器"""
        if not self.server_process:
            self.log_message("啟動伺服器...")
            self.status_label.config(text="伺服器狀態: 運行中", fg="green")
            self.start_button.config(state=tk.DISABLED)
            self.stop_button.config(state=tk.NORMAL)

            # 使用執行緒來啟動伺服器
            threading.Thread(target=self.run_server).start()

    def stop_server(self):
        """停止伺服器"""
        if self.server_process:
            self.log_message("正在停止伺服器...")
            self.server_process.terminate()
            # print(self.server_process.returncode)
            self.server_process = None
            self.status_label.config(text="伺服器狀態: 已停止", fg="red")
            self.start_button.config(state=tk.NORMAL)
            self.stop_button.config(state=tk.DISABLED)

    def run_server(self):
        """執行伺服器的子進程"""
        try:
            # 用 subprocess 執行您的 C++ 程式
            self.server_process = subprocess.Popen(
                ["server_new.exe", self.location_value],  # 這裡替換為您的伺服器執行檔路徑
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True
            )
            
            threading.Thread(target=self.monitor_server_process).start()
            
            # 讀取伺服器的輸出
            for line in self.server_process.stdout:
                self.log_message(line.strip())

            self.log_message("伺服器已停止。")
        except Exception as e:
            self.log_message(f"錯誤: {e}")
            self.status_label.config(text="伺服器狀態: 出錯", fg="red")
        finally:
            self.cleanup()

    def monitor_server_process(self):
        """持續檢查伺服器子進程的狀態"""
        self.server_process.wait()  # 等待進程結束
        if self.server_process is not None:
            if self.server_process.returncode is not None:  # 進程結束時執行
                self.log_message("伺服器自動停止或超時。")
                self.cleanup()
    
    def cleanup(self):
        """清理資源並更新狀態標籤"""
        self.server_process = None
        self.status_label.config(text="伺服器狀態: 已停止", fg="red")
        self.start_button.config(state=tk.NORMAL)
        self.stop_button.config(state=tk.DISABLED)

# 主程式
if __name__ == "__main__":
    root = tk.Tk()
    gui = ServerGUI(root)
    root.mainloop()
