package org.senseta.wiz.uipart;

import java.awt.Component;
import java.awt.Container;
import java.awt.Dimension;
import java.awt.Frame;
import java.awt.Insets;
import java.awt.LayoutManager;
import java.awt.Dialog.ModalityType;
import java.awt.event.ActionEvent;
import java.awt.event.ActionListener;
import java.awt.event.KeyEvent;
import java.awt.event.WindowAdapter;
import java.awt.event.WindowEvent;
import java.io.File;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Enumeration;
import java.util.List;

import javax.swing.AbstractCellEditor;
import javax.swing.ImageIcon;
import javax.swing.JButton;
import javax.swing.JComponent;
import javax.swing.JDialog;
import javax.swing.JPanel;
import javax.swing.JRootPane;
import javax.swing.JScrollPane;
import javax.swing.JTree;
import javax.swing.KeyStroke;
import javax.swing.filechooser.FileSystemView;
import javax.swing.tree.DefaultMutableTreeNode;
import javax.swing.tree.TreeCellEditor;
import javax.swing.tree.TreeCellRenderer;
import javax.swing.tree.TreeNode;
import javax.swing.tree.TreeSelectionModel;

import org.senseta.wiz.ImageCache;
import org.senseta.wiz.uipart.TristateCheckbox.CheckState;

public class FileSelectionTree extends JPanel implements LayoutManager
{
	private static final long serialVersionUID = 0L;

	private FileSystemView view;
	
	private JTree tree;
	private JScrollPane scroll;
	
	public FileSelectionTree(File root)
	{
		view = FileSystemView.getFileSystemView();
		
		tree = new JTree(new FileNode(root, CheckState.UNCHECKED));
		tree.setRootVisible(true);
		tree.getSelectionModel().setSelectionMode(TreeSelectionModel.SINGLE_TREE_SELECTION);
		tree.setCellRenderer(new CellDisplay());
		tree.setCellEditor(new CellDisplay());
		tree.setEditable(true);
		
		scroll = new JScrollPane(tree);
		
		setLayout(this);
		add(scroll);
	}
	
	public File[] getSelectedFiles()
	{
		List<File> list = new ArrayList<File>();
		FileNode root = (FileNode)tree.getModel().getRoot();
		root.getSelectedFiles(list);
		
		return list.toArray(new File[list.size()]);
	}
	
	
	/*
	public void setSelectedFile(File file)
	{
		if (file == null)
		{
			file = view.getDefaultDirectory();
		}
		tree.setSelectionPath(mkPath(file));
	}
	*/
	

	/*
	private TreePath mkPath(File file)
	{
		TreePath parentPath = null;
		Object root = tree.getModel().getRoot();
		if (root instanceof FileNode && (((FileNode)root).getFile().equals(file)))
		{
			return new TreePath(root);
		}
		else if (root instanceof RootListNode && (((RootListNode)root).hasFile(file)))
		{
			parentPath = new TreePath(root);
		}
		
		
		if (parentPath == null)
		{
			parentPath = mkPath(view.getParentDirectory(file));
		}
		
		DefaultMutableTreeNode parentNode = (DefaultMutableTreeNode)parentPath.getLastPathComponent();
		Enumeration<?> enumeration = parentNode.children();
		while (enumeration.hasMoreElements()) {
			FileNode child = (FileNode)enumeration.nextElement();
			if (child.getFile().equals(file)) {
				return parentPath.pathByAddingChild(child);		
			}
		}
		return null;
	}
	*/
	
	@Override
	public void layoutContainer(Container parent) {
		Insets in = parent.getInsets();
		int x = in.left, y = in.top;
		int w = parent.getWidth() - in.left - in.right;
		int h = parent.getHeight() - in.top - in.bottom;
		
		scroll.setBounds(x, y, w, h);
	}

	@Override public void addLayoutComponent(String name, Component comp) {}
	@Override public void removeLayoutComponent(Component comp) {}
	@Override public Dimension minimumLayoutSize(Container parent) { return new Dimension(0,0); }
	@Override public Dimension preferredLayoutSize(Container parent) { return minimumLayoutSize(parent); }
	
	private class CellDisplay extends AbstractCellEditor implements TreeCellEditor, TreeCellRenderer
	{
		private static final long serialVersionUID = 0L;
		private TristateCheckbox checkbox = null;
		private FileNode editing = null;
		
		@Override
		public Component getTreeCellEditorComponent(JTree tree, Object value, boolean isSelected, boolean expanded, boolean leaf, int row) {
			if (checkbox == null)
			{
				checkbox = new TristateCheckbox();
				checkbox.addActionListener(new ActionListener() {
					@Override
					public void actionPerformed(ActionEvent e) {
						editing.setState(checkbox.getState());
					}
				});
			}
			
			editing = (FileNode)value;
			editing.setState(CheckState.next(editing.state));
			checkbox.setState(editing.state);
			checkbox.setText(view.getSystemDisplayName(editing.getFile()));
			checkbox.setIcon(view.getSystemIcon(editing.getFile()));
			
			return checkbox;
		}

		@Override
		public Object getCellEditorValue() {
			return editing.getFile();
		}
		
		private TristateCheckbox label = new TristateCheckbox();
		
		@Override
		public Component getTreeCellRendererComponent(JTree tree, Object value, boolean selected, boolean expanded, boolean leaf, int row, boolean hasFocus) {
			FileNode n = (FileNode)value;
			label.setState(n.state);
			label.setText(view.getSystemDisplayName(n.getFile()));
			label.setIcon(view.getSystemIcon(n.getFile()));
			
			return label;
		}
	}
	
	private class FileNode extends DefaultMutableTreeNode
	{
		private static final long serialVersionUID = 0L;
		private CheckState state;
		
		private FileNode(File fp, CheckState state)
		{
			super(fp);
			this.state = state;
		}

		private File getFile() { return (File)userObject; }
		public int getChildCount() { populateChildren(); return super.getChildCount(); }
		public Enumeration<?> children() { populateChildren(); return super.children(); }
		
		public void getSelectedFiles(List<File> list)
		{
			if (state != CheckState.UNCHECKED)
			{
				list.add(getFile());
				populateChildren();
				
				Enumeration<?> enumeration = children();
				while (enumeration.hasMoreElements()) {
					FileNode child = (FileNode)enumeration.nextElement();
					child.getSelectedFiles(list);
				}
			}
		}
		
		public boolean isLeaf()
		{
			if (!getFile().isDirectory())
				return true;
			
			File[] files = getFile().listFiles();
			return !view.isTraversable(getFile()) || files == null || files.length == 0;
		}
		
		public void setState(CheckState newstate)
		{
			state = newstate;
			
			TreeNode n = this;
			while ((n = n.getParent()) != null)
			{
				((FileNode)n).updateState();
			}
			
			setChildState(this, newstate);
			
			tree.repaint();
		}
		
		private void setChildState(FileNode n, CheckState newstate)
		{
			if (n.children == null)
				return;
			
			Enumeration<?> enumeration = n.children();
			while (enumeration.hasMoreElements()) {
				FileNode child = (FileNode)enumeration.nextElement();
				child.state = newstate;
				setChildState(child, newstate);
			}
		}
		
		private void updateState()
		{
			boolean oneselected = false;
			boolean allselected = true;
			
			Enumeration<?> enumeration = children();
			while (enumeration.hasMoreElements()) {
				FileNode child = (FileNode)enumeration.nextElement();
				
				if (child.state != CheckState.UNCHECKED)
					oneselected = true;
				
				if (child.state != CheckState.CHECKED)
					allselected = false;
			}
			
			if (allselected)
				state = CheckState.CHECKED;
			else if (oneselected)
				state = CheckState.PARTIAL;
			else
				state = CheckState.UNCHECKED;
		}
		
		private void populateChildren() {
			if (children == null) {
				File[] files = view.getFiles(getFile(), true);
				Arrays.sort(files);
				for (File f : files) {
					insert(new FileNode(f, state), (children == null) ? 0 : children.size());
				}
			}
		}
	}
}
