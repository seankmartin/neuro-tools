# -*- coding: utf-8 -*-
"""
Plots and analysis for MEDPC behaviour.

@author: HAMG
"""

import matplotlib.pyplot as plt
import numpy as np
import math
import os.path
from bv_utils import mycolors


def cumplot(session, out_dir, ax=None, smooth=False, zoom=False, zoom_sch=False, title=False):
    """Perform a cumulative plot for a Session."""
    date = session.get_metadata('start_date').replace('/', '_')
    timestamps = session.get_arrays()
    lever_ts = session.get_lever_ts()
    session_type = session.get_metadata('name')
    subject = session.get_metadata('subject')

    # You have the array sorted, no need to histogram
    reward_times = timestamps["Nosepoke"]
    single_plot = False
    if ax is None:
        single_plot = True
        fig, ax = plt.subplots()
        ax.set_title('Cumulative Lever Presses\n', fontsize=15)
        if session_type == '5a_FixedRatio_p':
            ratio = int(timestamps["Experiment Variables"][3])
            plt.suptitle('\nSubject {}, {} {}, {}'.format(
                subject, session_type[:-2], ratio, date),
                fontsize=9, y=.98, x=.51)
        elif session_type == '5b_FixedInterval_p':
            interval = int(timestamps["Experiment Variables"][3] / 100)
            plt.suptitle('\nSubject {}, {} {}s, {}'.format(
                subject, session_type[:-2], interval, date),
                fontsize=9, y=.98, x=.51)
        elif session_type == '6_RandomisedBlocks_p:':
            ratio = int(timestamps["Experiment Variables"][3])
            interval = int(timestamps["Experiment Variables"][5] / 100)
            plt.suptitle('\nSubject {}, {} FR{}/FI{}s, {}'.format(
                subject, session_type[:-2], ratio, interval, date),
                fontsize=9, y=.98, x=.51)
        else:
            plt.suptitle('\nSubject {}, {}, {}'.format(
                subject, session_type[:-2], date),
                fontsize=9, y=.98, x=.51)
    else:
        if session_type == '5a_FixedRatio_p':
            ratio = int(timestamps["Experiment Variables"][3])
            ax.set_title('\nSubject {}, FR{}, {}'.format(
                subject, ratio, date),
                fontsize=12)
        elif session_type == '5b_FixedInterval_p':
            interval = int(timestamps["Experiment Variables"][3] / 100)
            ax.set_title('\nSubject {}, FI{}s, {}'.format(
                subject, interval, date),
                fontsize=12)
        elif session_type == '6_RandomisedBlocks_p':
            switch_ts = np.arange(5, 1830, 305)
            for x in switch_ts:
                plt.axvline(x, color='g', linestyle='-.', linewidth='.4')
            ratio = int(timestamps["Experiment Variables"][3])
            interval = int(timestamps["Experiment Variables"][5] / 100)
            ax.set_title('\nSubject {}, FR{}/FI{}s, {}'.format(
                subject, ratio, interval, date),
                fontsize=12)

    ax.set_xlabel('Time (s)')
    ax.set_ylabel('Cumulative Lever Presses')

    if smooth:
        values, base = np.histogram(lever_ts, bins=len(lever_ts) * 4)
        cumulative = np.cumsum(values)
        plot_arr_x = base[:-1]
        plot_arr_y = cumulative
        if reward_times[-1] > base[-1]:
            plot_arr_x = np.append(plot_arr_x, reward_times[-1])
            plot_arr_y = np.append(plot_arr_y, cumulative[-1])

        ax.plot(plot_arr_x, plot_arr_y, c=mycolors(subject))
        bins = base[:-1]

    elif zoom:
        trial_lever_ts = np.split(lever_ts,np.searchsorted(lever_ts,reward_times))
        norm_reward_ts = []
        norm_lever_ts = []
        reward_times_0 = np.append([0], reward_times, axis=0)
        for i, l in enumerate(trial_lever_ts[:-1]):
            norm_lever_ts.append(np.append([0], l-reward_times_0[i], axis=0))
            norm_reward_ts.append(reward_times[i]-reward_times_0[i])
        ax.set_xlim(0, np.max(norm_reward_ts))
        color = plt.cm.get_cmap('autumn')
        for i, l in enumerate(norm_lever_ts):
            ax.step(l, np.arange(l.size), c=color(i*20), where="post")
            bins = l
            reward_y = np.digitize(norm_reward_ts[i], bins) - 1
            plt.scatter(norm_reward_ts[i], reward_y, marker="x", c="grey", s=25)
        ax.set_title('\nSubject {}, Trial-Based'.format(
                subject), fontsize=12)
        ax.legend()
        return

    elif zoom_sch and session_type == '6_RandomisedBlocks_p':  # plots cum graph based on schedule type (i.e. FI/FR)
        sch_type = session.get_arrays('Trial Type')
        sch_switch = np.arange(5, 1830, 305)
        sch_lever_ts = np.split(lever_ts,np.searchsorted(lever_ts, sch_switch))
        sch_reward_ts = np.split(reward_times,np.searchsorted(reward_times, sch_switch))
        norm_reward_ts = []
        norm_lever_ts = []
        ratio_c = plt.cm.get_cmap('autumn')
        interval_c = plt.cm.get_cmap('winter')
        for i, l in enumerate(sch_lever_ts[1:]):
            norm_lever_ts.append(np.append([0], l-sch_switch[i], axis=0))
            norm_reward_ts.append(sch_reward_ts[i+1]-sch_switch[i])
        ax.set_xlim(0, 305)
        for i, l in enumerate(norm_lever_ts):
            if sch_type[i] == 1:
                ax.step(l, np.arange(l.size), c=ratio_c(i*45),
                        where="post", label='B'+str(i+1)+' - FR')
            else:
                ax.step(l, np.arange(l.size), c=interval_c(i*45),
                        where="post", label='B'+str(i+1)+' - FI')
            bins = l
            reward_y = np.digitize(norm_reward_ts[i], bins) - 1
            plt.scatter(norm_reward_ts[i], reward_y, marker="x", c="grey", s=25)
        ax.set_title('\nSubject {}, Schedule-Based'.format(
                subject), fontsize=12)
        ax.legend()
        return
    
    elif zoom_sch:  # plots cum graph based on schedule type (i.e. FI/FR)
        if session_type == '5a_FixedRatio_p':
            sch_type = 'FR'
            ratio = int(timestamps["Experiment Variables"][3])
            ax.set_title('\nSubject {}, FR{} Split'.format(
                subject, ratio),
                fontsize=12)
        elif session_type == '5b_FixedInterval_p':
            sch_type = 'FI'
            interval = int(timestamps["Experiment Variables"][3] / 100)
            ax.set_title('\nSubject {}, FI{}s Split'.format(
                subject, interval),
                fontsize=12)
        blocks = np.arange(0, 60*30, 300)  # Change values to set division blocks
        split_lever_ts = np.split(lever_ts,np.searchsorted(lever_ts, blocks))
        split_reward_ts = np.split(reward_times,np.searchsorted(reward_times, blocks))
        norm_reward_ts = []
        norm_lever_ts = []
        for i, l in enumerate(split_lever_ts[1:]):
            norm_lever_ts.append(np.append([0], l-blocks[i], axis=0))
            norm_reward_ts.append(split_reward_ts[i+1]-blocks[i])
        ax.set_xlim(0, 305)
        for i, l in enumerate(norm_lever_ts):
            ax.step(l, np.arange(l.size), c=mycolors(i), where="post", 
                    label='B'+str(i+1)+' - {}'.format(sch_type))
            bins = l
            reward_y = np.digitize(norm_reward_ts[i], bins) - 1
            plt.scatter(norm_reward_ts[i], reward_y, marker="x", c="grey", s=25)
        ax.legend()
        return

    else:
        lever_times = np.insert(lever_ts, 0, 0, axis=0)
        ax.step(lever_times, np.arange(
            lever_times.size), c=mycolors(subject), where="post", label='Animal'+subject)
        if reward_times[-1] > lever_times[-1]:
            ax.plot(
                [lever_times[-1], reward_times[-1] + 2],
                [lever_times.size - 1, lever_times.size - 1],
                c=mycolors(subject))
        bins = lever_times
        reward_y = np.digitize(reward_times, bins) - 1

    if smooth:
        reward_y = cumulative[reward_y]
        
    plt.scatter(reward_times, reward_y, marker="x", c="grey",
                label='Reward Collected', s=25)
    ax.legend()
#    ax.set_xlim(0, 30 * 60 + 30)

    if single_plot:
        out_name = (subject.zfill(3) + "_CumulativeHist_" +
                    session_type[:-2] + "_" + date + ".png")
        out_name = os.path.join(out_dir, out_name)
        print("Saved figure to {}".format(out_name))
        # Text Display on Graph
        ax.text(0.55, 0.15, 'Total # of Lever Press: {}\nTotal # of Rewards: {}'
                .format(len(lever_ts), len(reward_times)), transform=ax.transAxes)
        fig.savefig(out_name, dpi=400)
        plt.close()
    else:
        # Text Display on Graph
        ax.text(0.43, 0.2, 'Total # of Lever Press: {}\nTotal # of Rewards: {}'
                .format(len(lever_ts), len(reward_times)), transform=ax.transAxes)
        return


def IRT(session, out_dir, showIRT=False, ax=None):
    """Perform an inter-response time plot for a Session."""
    date = session.get_metadata('start_date').replace('/', '_')
    time_taken = session.time_taken()
    timestamps = session.get_arrays()
    good_lever_ts = session.get_lever_ts(False)
    session_type = session.get_metadata('name')
    subject = session.get_metadata('subject')
    single_plot = False
    
    rewards_i = timestamps["Reward"]
    nosepokes_i = timestamps["Nosepoke"]
    # Session ended w/o reward collection
    if len(rewards_i) > len(nosepokes_i):
        # Assumes reward collected at end of session
        nosepokes_i = np.append(
            nosepokes_i, [timestamps["Experiment Variables"][0] * 60])
    # Only consider after the first lever press
    reward_idxs = np.nonzero(rewards_i >= good_lever_ts[0])
    rewards = rewards_i[reward_idxs]
    nosepokes = nosepokes_i[reward_idxs]
    # b assigns ascending numbers to rewards within lever presses
    b = np.digitize(rewards, bins=good_lever_ts)
    _, a = np.unique(b, return_index=True)  # returns index for good rewards
    good_nosepokes = nosepokes[a]  # nosepoke ts for pressing levers
    if session_type == '5a_FixedRatio_p':
        ratio = int(timestamps["Experiment Variables"][3])
        good_lever_ts = good_lever_ts[::ratio]
    if len(good_lever_ts[1:]) > len(good_nosepokes[:-1]):
        IRT = good_lever_ts[1:] - good_nosepokes[:]  # Ended sess w lever press
    else:
        # Ended session w nosepoke
        IRT = good_lever_ts[1:] - good_nosepokes[:-1]

    hist_count, hist_bins, _ = ax.hist(
        IRT, bins=math.ceil(np.amax(IRT)),
        range=(0, math.ceil(np.amax(IRT))), color=mycolors(subject))

    # Plotting of IRT Graphs
    if ax is None:
        single_plot = True
        fig, ax = plt.subplots()
        ax.set_title('Inter-Response Time\n', fontsize=15)
        if session_type == '5a_FixedRatio_p':
            plt.suptitle('\n(Subject {}, {} {}, {})'.format(
                subject, session_type[:-2], ratio, date),
                fontsize=9, y=.98, x=.51)
        elif session_type == '5b_FixedInterval_p':
            interval = int(timestamps["Experiment Variables"][3] / 100)
            plt.suptitle('\n(Subject {}, {} {}s, {})'.format(
                subject, session_type[:-2], interval, date),
                fontsize=9, y=.98, x=.51)
        else:
            plt.suptitle('\n(Subject {}, {}, {})'.format(
                subject, session_type[:-2], date),
                fontsize=9, y=.98, x=.51)
        
    ax.set_xlabel('IRT (s)')
    ax.set_ylabel('Counts')
    maxidx = np.argmax(np.array(hist_count))
    maxval = (hist_bins[maxidx + 1] - hist_bins[maxidx]) / \
        2 + hist_bins[maxidx]

    if showIRT:
        show_IRT_details(IRT, maxidx, hist_bins)
    if single_plot:    
        # Text Display on Graph
        ax.text(0.55, 0.8, 'Session Duration: {} mins\nMost Freq. IRT Bin: {} s'
                .format(time_taken, maxval), transform=ax.transAxes)
        out_name = (subject.zfill(3) + "_IRT_Hist_" +
                    session_type[:-2] + "_" + date + ".png")
        print("Saved figure to {}".format(
            os.path.join(out_dir, out_name)))
        fig.savefig(os.path.join(out_dir, out_name), dpi=400)
        plt.close()
    else:
        return ax

def show_IRT_details(IRT, maxidx, hist_bins):
    """Display further information for an IRT."""
    plt.show()
    print('Most Freq. IRT Bin: {} s'.format((hist_bins[maxidx + 1] -
                                             hist_bins[maxidx]) / 2 + hist_bins[maxidx]))
    print(
        'Median Inter-Response Time (IRT): {0:.2f} s'.format(np.median(IRT)))
    print('Min IRT: {0:.2f} s'.format(np.amin(IRT)))
    print('Max IRT: {0:.2f} s'.format(np.amax(IRT)))
    print('IRTs: ', np.round(IRT, decimals=2))
