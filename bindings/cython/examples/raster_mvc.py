from dataclasses import dataclass

import numpy as np

import datoviz as dviz

# -------------------------------------------------------------------------------------------------
# Raster viewer
# -------------------------------------------------------------------------------------------------


class RasterView:
    def __init__(self):
        self.canvas = dviz.canvas(show_fps=True)
        self.panel = self.canvas.panel(controller='axes')
        self.visual = self.panel.visual('point')
        self.pvars = {'ms': 2., 'alpha': .03}
        self.gui = self.canvas.gui('XY')
        self.gui.control("label", "Coords", value="(0, 0)")

    def set_spikes(self, spikes):
        pos = np.c_[spikes['times'], spikes['depths'], np.zeros_like(spikes['times'])]
        color = dviz.colormap(20 * np.log10(spikes['amps']), cmap='cividis', alpha=self.pvars['alpha'])
        self.visual.data('pos', pos)
        self.visual.data('color', color)
        self.visual.data('ms', np.array([self.pvars['ms']]))


class RasterController:
    _time_select_cb = None

    def __init__(self, model, view):
        self.m = model
        self.v = view
        self.v.canvas.connect(self.on_mouse_move)
        self.v.canvas.connect(self.on_key_press)
        self.redraw()

    def redraw(self):
        print('redraw', self.v.pvars)
        self.v.set_spikes(self.m.spikes)

    def on_mouse_move(self, x, y, modifiers=()):
        p = self.v.canvas.panel_at(x, y)
        if not p:
            return
        # Then, we transform into the data coordinate system
        # Supported coordinate systems:
        #   target_cds='data' / 'scene' / 'vulkan' / 'framebuffer' / 'window'
        xd, yd = p.pick(x, y)
        self.v.gui.set_value("Coords", f"({xd:0.2f}, {yd:0.2f})")

    def on_key_press(self, key, modifiers=()):
        print(key, modifiers)
        if key == 'a' and modifiers == ('control',):
            self.v.pvars['alpha'] = np.minimum(self.v.pvars['alpha'] + 0.1, 1.)
        elif key == 'z' and modifiers == ('control',):
            self.v.pvars['alpha'] = np.maximum(self.v.pvars['alpha'] - 0.1, 0.)
        elif key == 'page_up':
            self.v.pvars['ms'] = np.minimum(self.v.pvars['ms'] * 1.1, 20)
        elif key == 'page_down':
            self.v.pvars['ms'] = np.maximum(self.v.pvars['ms'] / 1.1, 1)
        else:
            return
        self.redraw()


def multiple_spike_trains(firing_rates=None, rec_len_secs=1000, depths=None,
                          amplitude_noise=20 * 1e-6):
    """
    :param firing_rates: list or np.array of firing rates (spikes per second)
    :param rec_len_secs: recording length in seconds
    :return: spike_times, spike_amps, spike_clusters
    """
    cluster_ids = np.arange(firing_rates.size)
    ca = np.exp(np.random.normal(5.5, 0.5, firing_rates.size)) / 1e6  # output is in V
    nspi = int(np.sum(firing_rates * rec_len_secs * 1.25))
    st = np.zeros(nspi)
    sc = np.zeros(nspi, dtype=np.int32)
    sa = np.zeros(nspi)
    sd = np.zeros(nspi)
    ispi = 0
    for i, firing_rate in enumerate(firing_rates):
        t = generate_spike_train(firing_rate=firing_rate, rec_len_secs=rec_len_secs)
        st[ispi:ispi + t.size] = t
        sa[ispi:ispi + t.size] = ca[i]
        sc[ispi:ispi + t.size] = np.int32(cluster_ids[i])
        sd[ispi:ispi + t.size] = float(np.random.randint(40, 3600)) + np.random.randn(t.size) * 10
        ispi += t.size

    ordre = st[:ispi].argsort()
    st = st[ordre]
    sa = sa[ordre]
    sd = sd[ordre]
    # sa += np.random.randn(st.size) * amplitude_noise
    return st, sa, sc, sd


def generate_spike_train(firing_rate=200, rec_len_secs=1000):
    """
    Basic spike train generator following a poisson process for spike-times and
    :param firing_rate:
    :param rec_len_secs:
    :return: spike_times (secs) , spike_amplitudes (V)
    """
    # spike times: exponential decay prob
    st = np.cumsum(- np.log(np.random.rand(int(rec_len_secs * firing_rate * 1.5))) / firing_rate)
    st = st[:np.searchsorted(st, rec_len_secs)]
    return st

@dataclass
class RasterModel:
    spikes: dict


def raster(spikes):
    rm = RasterController(RasterModel(spikes), RasterView())
    dviz.run()


NCELLS = 400
RECLEN = 1000.
frs = np.random.randint(5, 80, NCELLS)
spikes = {}
spikes['times'], spikes['amps'], spikes['clusters'], spikes['depths'] = multiple_spike_trains(
    firing_rates=frs, rec_len_secs=RECLEN)

drift_ = np.sin(2 * np.pi * spikes['times'] / RECLEN * 2) * 50
spikes['depths'] += drift_
print(len(spikes['times']))

raster(spikes)
